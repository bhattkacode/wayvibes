#ifndef AUDIO_H
#define AUDIO_H

#include "config.h"
#include "miniaudio.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Global audio engine instance
extern ma_engine engine;

ma_result initializeAudioEngine();
void playSound(const std::string &soundFile);
void setVolume(float volume);
void runMainLoop(const std::string &devicePath,
                 const std::unordered_map<int, std::string> &keySoundMap, float volume,
                 const std::string &soundpackPath);

// --- V2: decode once, pre-slice into RAM ---

// One pre-sliced timing window. PCM is interleaved float32 at the engine rate.
struct Clip {
  std::vector<float> pcm;
  uint32_t channels = 0;
  uint32_t sampleRate = 0;
  bool empty() const { return pcm.empty(); }
};

struct KeySound {
  Clip press;
  Clip release;
};

// Decode pack.audioFile (resolved against soundpackDir) and slice every timing window
// into its own Clip. Frees the full decode after slicing. Returns false + err when
// nothing could be loaded. Warns (but continues) on clamped timings and multi fallback.
bool loadV2Clips(const V2Pack &pack, const std::string &soundpackDir,
                 std::unordered_map<int, KeySound> &out,
                 std::unordered_map<int, std::string> &fallbackFiles, std::string &err);

// V1: probe every unique file in the map, offer one batch ffmpeg convert for
// undecodable oggs, patch config.json + map. Warn-only, never fatal.
void ensureV1FilesPlayable(const std::string &configPath, const std::string &soundpackDir,
                           std::unordered_map<int, std::string> &keySoundMap);

// Play one clip from memory through the voice pool (polyphonic, oldest evicted past 32).
// Engine volume applies, pitch jitter applies when randomPitch is set.
void playClip(const Clip &clip, bool randomPitch);

void runMainLoopV2(const std::string &devicePath,
                   const std::unordered_map<int, KeySound> &keySounds,
                   const std::unordered_map<int, std::string> &fallbackFiles, float volume,
                   bool randomPitch);

#endif // AUDIO_H
