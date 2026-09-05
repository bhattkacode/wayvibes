#define MINIAUDIO_IMPLEMENTATION
#include "audio.h"
#include "miniaudio.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <nlohmann/json.hpp>
#include <random>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>

ma_engine engine;

ma_result initializeAudioEngine() { return ma_engine_init(NULL, &engine); }

void uninitializeAudioEngine() { ma_engine_uninit(&engine); }

void playSound(const std::string &soundFile) {
  if (ma_engine_play_sound(&engine, soundFile.c_str(), NULL) != MA_SUCCESS) {
    std::cerr << "Error playing sound: " << soundFile << std::endl;
  }
}

void setVolume(float volume) { ma_engine_set_volume(&engine, volume); }

// Open the evdev node, print what we are listening on. Returns the fd or -1.
static int openInputDevice(const std::string &devicePath, std::string &nameOut) {
  int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::cerr << "Failed to open input device: " << devicePath << std::endl;
    return -1;
  }
  char name[256] = "Unknown";
  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  nameOut = name;
  std::cout << "Listening for key events on: " << nameOut << " (" << devicePath << ")"
            << std::endl;
  return fd;
}

void runMainLoop(const std::string &devicePath,
                 const std::unordered_map<int, std::string> &keySoundMap, float volume,
                 const std::string &soundpackPath) {
  std::string devName;
  int fd = openInputDevice(devicePath, devName);
  if (fd < 0) return;
  setVolume(volume);

  struct input_event ev;
  while (true) {
    ssize_t n = read(fd, &ev, sizeof(ev));
    if (n == sizeof(ev)) {
      if (ev.type == EV_KEY && ev.value == 1) { // key press
        auto it = keySoundMap.find(ev.code);
        if (it != keySoundMap.end()) {
          std::string soundFile = soundpackPath + "/" + it->second;
          playSound(soundFile);
        }
      }
    } else {
      usleep(1000); // sleep 1ms to avoid busy loop
    }
  }

  close(fd);
}

// --- V2: decode once, pre-slice ---

static bool decodeAudioFile(const std::string &path, uint32_t targetRate,
                            std::vector<float> &outPcm, uint32_t &outChannels,
                            uint32_t &outRate, std::string &err) {
  ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, targetRate);
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), &cfg, &decoder) != MA_SUCCESS) {
    err = "Could not decode audio file: " + path +
          " (missing file or unsupported codec; convert ogg to wav with ffmpeg).";
    return false;
  }

  ma_format fmt;
  ma_uint32 channels, sampleRate;
  ma_decoder_get_data_format(&decoder, &fmt, &channels, &sampleRate, NULL, 0);
  outChannels = channels;
  outRate = sampleRate;

  std::vector<float> chunk(4096 * std::max<ma_uint32>(channels, 1));
  while (true) {
    ma_uint64 framesRead = 0;
    ma_result res = ma_decoder_read_pcm_frames(&decoder, chunk.data(), 4096, &framesRead);
    if (framesRead > 0) {
      outPcm.insert(outPcm.end(), chunk.begin(),
                    chunk.begin() + framesRead * channels);
    }
    if (res != MA_SUCCESS || framesRead < 4096) break;
  }
  ma_decoder_uninit(&decoder);

  if (outPcm.empty()) {
    err = "No decodable audio in file: " + path;
    return false;
  }
  return true;
}

// --- ffmpeg fallback for oggs miniaudio can't decode ---

static std::string lowerStr(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

static bool isYes(const std::string &answer) {
  return answer == "y" || answer == "Y" || answer == "yes" || answer == "YES";
}

static bool hasOggExt(const std::string &path) {
  std::string ext = lowerStr(std::filesystem::path(path).extension().string());
  return ext == ".ogg" || ext == ".oga";
}

static std::string withWavExt(const std::string &path) {
  return std::filesystem::path(path).replace_extension(".wav").string();
}

static bool ffmpegAvailable() {
  return std::system("ffmpeg -hide_banner -version > /dev/null 2>&1") == 0;
}

// No shell, so pack paths with spaces or quotes can't inject commands.
static bool runFfmpegConvert(const std::string &inPath, const std::string &outPath) {
  pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    execlp("ffmpeg", "ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i",
           inPath.c_str(), outPath.c_str(), (char *)NULL);
    _exit(127);
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
         std::filesystem::exists(outPath);
}

static bool probeDecodable(const std::string &path) {
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), NULL, &decoder) != MA_SUCCESS) return false;
  ma_decoder_uninit(&decoder);
  return true;
}

// One-time convert: ogg -> wav next to it, patch audio_file in config.json so this
// never asks again. Returns true when a usable wav path was produced.
static bool autoConvertOgg(const std::string &audioPath, const std::string &soundpackDir,
                           const std::string &audioField, std::string &wavPathOut) {
  if (!hasOggExt(audioPath) || !std::filesystem::exists(audioPath)) return false;
  if (!ffmpegAvailable()) {
    std::cerr << "ffmpeg not found, install it to auto-convert this file." << std::endl;
    return false;
  }
  if (!isatty(STDIN_FILENO)) {
    std::cerr << "Run once in a terminal to auto-convert this file with ffmpeg."
              << std::endl;
    return false;
  }

  std::cout << "Built-in decoder can't read '" << audioField
            << "'. Convert to wav with ffmpeg? (one-time, updates config.json) [y/N]: "
            << std::flush;
  std::string answer;
  std::getline(std::cin, answer);
  if (!isYes(answer)) return false;

  std::string wavPath = withWavExt(audioPath);
  if (!std::filesystem::exists(wavPath)) {
    if (!runFfmpegConvert(audioPath, wavPath)) {
      std::cerr << "ffmpeg conversion failed." << std::endl;
      return false;
    }
    std::cout << "Converted to " << wavPath << std::endl;
  }

  // Point the pack at the wav so next launch skips all of this.
  std::string configPath = soundpackDir + "/config.json";
  std::ifstream in(configPath);
  if (in.is_open()) {
    try {
      nlohmann::json cfg;
      in >> cfg;
      in.close();
      cfg["audio_file"] = withWavExt(audioField);
      std::ofstream outFile(configPath);
      if (outFile.is_open()) {
        outFile << cfg.dump(2);
        std::cout << "Updated audio_file in config.json" << std::endl;
      }
    } catch (...) {
      std::cerr << "Warning: could not update config.json, using converted file "
                   "for this run only."
                << std::endl;
    }
  }
  wavPathOut = wavPath;
  return true;
}

// Short fades so arbitrary slice cuts don't click. Per-frame so channels stay in phase.
static void applyEdgeFade(std::vector<float> &pcm, uint32_t channels, uint32_t sampleRate) {
  if (pcm.empty() || channels == 0 || sampleRate == 0) return;
  uint64_t frames = pcm.size() / channels;
  if (frames < 2) return;
  uint64_t fadeIn = (uint64_t)(2.0 * sampleRate / 1000.0);
  uint64_t fadeOut = (uint64_t)(5.0 * sampleRate / 1000.0);
  fadeIn = std::min(fadeIn, frames / 2);
  fadeOut = std::min(fadeOut, frames / 2);
  for (uint64_t f = 0; f < fadeIn; f++) {
    float gain = (float)f / (float)fadeIn;
    for (uint32_t c = 0; c < channels; c++) pcm[f * channels + c] *= gain;
  }
  for (uint64_t f = 0; f < fadeOut; f++) {
    float gain = (float)f / (float)fadeOut;
    uint64_t idx = frames - 1 - f;
    for (uint32_t c = 0; c < channels; c++) pcm[idx * channels + c] *= gain;
  }
}

static bool sliceWindow(const std::vector<float> &full, uint32_t channels,
                        uint32_t sampleRate, const V2Timing &w, int code, Clip &out) {
  uint64_t totalFrames = full.size() / channels;
  double durationMs = (double)totalFrames * 1000.0 / (double)sampleRate;
  uint64_t startFrame = (uint64_t)(w.startMs * sampleRate / 1000.0);
  uint64_t endFrame = (uint64_t)(w.endMs * sampleRate / 1000.0);
  if (startFrame >= totalFrames) {
    std::cerr << "Warning: timing start " << w.startMs << "ms past file end ("
              << durationMs << "ms) for code " << code << ", skipping." << std::endl;
    return false;
  }
  if (endFrame > totalFrames) {
    std::cerr << "Warning: timing end " << w.endMs << "ms beyond file duration ("
              << durationMs << "ms) for code " << code << ", clamping." << std::endl;
    endFrame = totalFrames;
  }
  if (endFrame <= startFrame) return false;
  out.channels = channels;
  out.sampleRate = sampleRate;
  out.pcm.assign(full.begin() + startFrame * channels, full.begin() + endFrame * channels);
  applyEdgeFade(out.pcm, channels, sampleRate);
  return !out.pcm.empty();
}

bool loadV2Clips(const V2Pack &pack, const std::string &soundpackDir,
                 std::unordered_map<int, KeySound> &out,
                 std::unordered_map<int, std::string> &fallbackFiles, std::string &err) {
  out.clear();
  fallbackFiles.clear();
  std::vector<float> full;
  uint32_t channels = 0, sampleRate = 0;
  bool haveAudio = false;

  if (!pack.audioFile.empty()) {
    std::string audioPath = soundpackDir + "/" + pack.audioFile;
    uint32_t engineRate = ma_engine_get_sample_rate(&engine);
    std::string decErr;
    if (!decodeAudioFile(audioPath, engineRate, full, channels, sampleRate, decErr)) {
      std::string wavPath;
      if (autoConvertOgg(audioPath, soundpackDir, pack.audioFile, wavPath)) {
        full.clear();
        decErr.clear();
        if (!decodeAudioFile(wavPath, engineRate, full, channels, sampleRate, decErr)) {
          decErr = "Converted wav still won't decode: " + wavPath;
        }
      }
    }
    if (!full.empty()) {
      haveAudio = true;
    } else if (pack.perKeyFiles.empty()) {
      err = decErr;
      return false;
    } else {
      std::cerr << "Warning: " << decErr << " Falling back to per-key files."
                << std::endl;
    }
  }

  if (haveAudio) {
    for (auto &[code, windows] : pack.timings) {
      KeySound ks;
      if (windows.size() >= 1) sliceWindow(full, channels, sampleRate, windows[0], code,
                                           ks.press);
      if (windows.size() >= 2) sliceWindow(full, channels, sampleRate, windows[1], code,
                                           ks.release);
      if (!ks.press.empty() || !ks.release.empty()) out[code] = std::move(ks);
    }
    // Full decode no longer needed, clips own their slices.
    full.clear();
    full.shrink_to_fit();
  } else if (!pack.timings.empty() && pack.perKeyFiles.empty()) {
    std::cerr << "Warning: no audio_file to slice and no per-key files, timed "
                 "keys will be silent."
              << std::endl;
  }

  // Multi-method fallback: per-key whole files, played V1-style.
  for (auto &[code, file] : pack.perKeyFiles) {
    std::string path = soundpackDir + "/" + file;
    if (!std::filesystem::exists(path)) {
      std::cerr << "Warning: per-key audio file missing: " << path << ", skipping."
                << std::endl;
      continue;
    }
    fallbackFiles[code] = path;
  }

  if (out.empty() && fallbackFiles.empty()) {
    err = "No decodable audio: sliced 0 clips and found 0 per-key files.";
    return false;
  }
  return true;
}

// --- V1: probe every file once, batch-convert broken oggs ---

void ensureV1FilesPlayable(const std::string &configPath, const std::string &soundpackDir,
                           std::unordered_map<int, std::string> &keySoundMap) {
  std::unordered_map<std::string, std::string> toWav; // ogg file -> wav file
  std::vector<std::string> missing, broken;
  std::unordered_set<std::string> seen;
  for (const auto &kv : keySoundMap) {
    const std::string &file = kv.second;
    if (!seen.insert(file).second) continue; // packs reuse wavs across keys
    std::string full = soundpackDir + "/" + file;
    if (!std::filesystem::exists(full)) {
      missing.push_back(file);
      continue;
    }
    if (probeDecodable(full)) continue;
    if (hasOggExt(file)) {
      toWav[file] = withWavExt(file);
    } else {
      broken.push_back(file);
    }
  }
  for (auto &f : missing) std::cerr << "Warning: sound file missing: " << f << std::endl;
  for (auto &f : broken)
    std::cerr << "Warning: can't decode '" << f << "', those keys will be silent."
              << std::endl;
  if (toWav.empty()) return;

  if (!ffmpegAvailable() || !isatty(STDIN_FILENO)) {
    std::cerr << "Warning: " << toWav.size()
              << " files need ffmpeg conversion (run once in a terminal with ffmpeg "
                 "installed), those keys will be silent."
              << std::endl;
    return;
  }

  std::cout << toWav.size()
            << " sound files can't be read by the built-in decoder (e.g. ";
  size_t shown = 0;
  for (const auto &kv : toWav) {
    const std::string &ogg = kv.first;
    if (shown++ >= 3) break;
    std::cout << (shown > 1 ? ", " : "") << ogg;
  }
  if (toWav.size() > 3) std::cout << ", ...";
  std::cout << "). Convert all to wav with ffmpeg? (one-time, updates config.json) [y/N]: "
            << std::flush;
  std::string answer;
  std::getline(std::cin, answer);
  if (!isYes(answer)) {
    std::cerr << "Warning: skipping conversion, those keys will be silent." << std::endl;
    return;
  }

  std::unordered_map<std::string, std::string> done;
  for (auto &[ogg, wav] : toWav) {
    std::string wavFull = soundpackDir + "/" + wav;
    if (std::filesystem::exists(wavFull) ||
        runFfmpegConvert(soundpackDir + "/" + ogg, wavFull)) {
      std::cout << "Converted " << ogg << std::endl;
      done[ogg] = wav;
    } else {
      std::cerr << "Warning: ffmpeg failed on " << ogg << std::endl;
    }
  }
  if (done.empty()) return;

  for (auto &kv : keySoundMap) {
    auto it = done.find(kv.second);
    if (it != done.end()) kv.second = it->second;
  }

  std::ifstream in(configPath);
  if (in.is_open()) {
    try {
      nlohmann::json cfg;
      in >> cfg;
      in.close();
      for (auto &[key, val] : cfg["defines"].items()) {
        if (!val.is_string()) continue;
        auto it = done.find(val.get<std::string>());
        if (it != done.end()) val = it->second;
      }
      std::ofstream outFile(configPath);
      if (outFile.is_open()) {
        outFile << cfg.dump(2);
        std::cout << "Updated defines in config.json" << std::endl;
      }
    } catch (...) {
      std::cerr << "Warning: could not update config.json, using converted files "
                   "for this run only."
                << std::endl;
    }
  }
}

// --- V2 voice pool: 32 voices, oldest evicted on overflow ---

namespace {
struct Voice {
  ma_audio_buffer buffer;
  ma_sound sound;
  bool active = false;
};
constexpr size_t MAX_VOICES = 32;
Voice voices[MAX_VOICES];
size_t nextVoice = 0;

float randomPitchFactor() {
  static thread_local std::mt19937 rng(std::random_device{}());
  static thread_local std::uniform_real_distribution<float> dist(0.9f, 1.1f);
  return dist(rng);
}
} // namespace

void playClip(const Clip &clip, bool randomPitch) {
  if (clip.empty() || clip.channels == 0) return;

  // Find a free voice, else evict the oldest.
  Voice *slot = nullptr;
  for (size_t i = 0; i < MAX_VOICES; i++) {
    Voice &v = voices[(nextVoice + i) % MAX_VOICES];
    if (!v.active || !ma_sound_is_playing(&v.sound)) {
      slot = &v;
      nextVoice = (&v - voices + 1) % MAX_VOICES;
      break;
    }
  }
  if (slot == nullptr) {
    slot = &voices[nextVoice];
    nextVoice = (nextVoice + 1) % MAX_VOICES;
  }
  if (slot->active) {
    ma_sound_uninit(&slot->sound);
    ma_audio_buffer_uninit(&slot->buffer);
    slot->active = false;
  }

  // Each voice gets its own buffer object over the shared immutable PCM, so
  // overlapping hits never share a cursor.
  uint64_t frames = clip.pcm.size() / clip.channels;
  ma_audio_buffer_config bufCfg = ma_audio_buffer_config_init(
      ma_format_f32, clip.channels, frames, clip.pcm.data(), NULL);
  if (ma_audio_buffer_init(&bufCfg, &slot->buffer) != MA_SUCCESS) return;
  if (ma_sound_init_from_data_source(&engine, &slot->buffer,
                                     MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
                                     &slot->sound) != MA_SUCCESS) {
    ma_audio_buffer_uninit(&slot->buffer);
    return;
  }
  if (randomPitch) ma_sound_set_pitch(&slot->sound, randomPitchFactor());
  if (ma_sound_start(&slot->sound) != MA_SUCCESS) {
    ma_sound_uninit(&slot->sound);
    ma_audio_buffer_uninit(&slot->buffer);
    return;
  }
  slot->active = true;
}

// Missing code fallback: exact -> Space -> Enter -> first loaded key -> silent.
static const Clip *findV2Clip(const std::unordered_map<int, KeySound> &keySounds,
                              int firstCode, int code, bool down) {
  int candidates[4] = {code, KEY_SPACE, KEY_ENTER, firstCode};
  for (int c : candidates) {
    if (c < 0) continue;
    auto it = keySounds.find(c);
    if (it == keySounds.end()) continue;
    const Clip &clip = down ? it->second.press : it->second.release;
    if (!clip.empty()) return &clip;
  }
  return nullptr;
}

// Same chain for multi-method per-key files, which only play on keydown.
static std::unordered_map<int, std::string>::const_iterator
findFallbackFile(const std::unordered_map<int, std::string> &fallbackFiles, int code) {
  auto it = fallbackFiles.find(code);
  if (it != fallbackFiles.end()) return it;
  if (code != KEY_SPACE) {
    it = fallbackFiles.find(KEY_SPACE);
    if (it != fallbackFiles.end()) return it;
  }
  if (code != KEY_ENTER) {
    it = fallbackFiles.find(KEY_ENTER);
    if (it != fallbackFiles.end()) return it;
  }
  return fallbackFiles.end();
}

void runMainLoopV2(const std::string &devicePath,
                   const std::unordered_map<int, KeySound> &keySounds,
                   const std::unordered_map<int, std::string> &fallbackFiles, float volume,
                   bool randomPitch) {
  std::string devName;
  int fd = openInputDevice(devicePath, devName);
  if (fd < 0) return;
  setVolume(volume);

  // Deterministic fallback: lowest evdev code with a press clip.
  int firstCode = -1;
  for (const auto &kv : keySounds) {
    if (!kv.second.press.empty() && (firstCode < 0 || kv.first < firstCode))
      firstCode = kv.first;
  }

  std::unordered_set<int> pressed;
  struct input_event ev;
  while (true) {
    ssize_t n = read(fd, &ev, sizeof(ev));
    if (n == sizeof(ev)) {
      if (ev.type != EV_KEY || ev.value == 2) continue; // no autorepeat
      if (ev.value == 1) { // keydown: timing[0]
        if (!pressed.insert(ev.code).second) continue; // duplicate down without up
        if (const Clip *clip = findV2Clip(keySounds, firstCode, ev.code, true)) {
          playClip(*clip, randomPitch);
        } else {
          // Multi-method per-key files play V1-style on keydown only.
          auto it = findFallbackFile(fallbackFiles, ev.code);
          if (it != fallbackFiles.end()) playSound(it->second);
        }
      } else if (ev.value == 0) { // keyup: timing[1], else silent
        if (pressed.erase(ev.code) == 0) continue; // up without down
        if (const Clip *clip = findV2Clip(keySounds, firstCode, ev.code, false)) {
          playClip(*clip, randomPitch);
        }
      }
    } else {
      usleep(1000); // sleep 1ms to avoid busy loop
    }
  }

  close(fd);
}
