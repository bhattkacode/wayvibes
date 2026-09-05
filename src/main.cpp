#include "audio.h"
#include "config.h"
#include "device.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <unordered_map>

void printHelp() {
  std::cout << "Usage: wayvibes [options] [soundpack_path]\n"
            << "Options:\n"
            << "  --device              Select input device\n"
            << "  --device-name <name>  Use input device by exact name\n"
            << "  -v <volume>           Set volume (0.0-10.0) (default: 1.0)\n"
            << "  --background, -bg     Run in background (detached from terminal)\n"
            << "  --help, -h            Show this help message\n"
            << "Note: default soundpack path is './' (current directory)\n"
            << "Example: wayvibes ~/wayvibes/akko_lavender_purples/ -v 3"
            << std::endl;
}

static float clampVolume(float v) { return std::clamp(v, 0.0f, 10.0f); }

int main(int argc, char *argv[]) {
  std::string soundpackPath = "./";
  float cliVolume = 1.0f;
  bool volumePassed = false;
  std::string configDir;
  std::string targetDeviceName = "";
  bool silent = false;
  const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
  configDir = (xdgConfigHome ? xdgConfigHome : std::string(getenv("HOME")) + "/.config") +
              "/wayvibes";

  if (!std::filesystem::exists(configDir)) {
    std::filesystem::create_directories(configDir);
  }

  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--device") {
      saveInputDevice(configDir);
      return 0;
    } else if ((std::string(argv[i]) == "--device-name") && (i + 1) < argc) {
      targetDeviceName = argv[i + 1];
      i++;
    } else if (std::string(argv[i]) == "-v" && (i + 1) < argc) {
      try {
        cliVolume = std::stof(argv[i + 1]);
        volumePassed = true;
        i++;
      } catch (...) {
        std::cerr << "Invalid volume argument. Using default volume(1.0)." << std::endl;
      }
    } else if (std::string(argv[i]) == "--background" || std::string(argv[i]) == "-bg") {
      silent = true;
    } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
      printHelp();
      return 0;
    } else if (argv[i][0] != '-') {
      soundpackPath = argv[i];
    } else {
      std::cerr << "Unknown argument: " << argv[i] << std::endl;
      printHelp();
      return 1;
    }
  }

  if (silent) {
    pid_t pid = fork();
    if (pid < 0) {
      std::cerr << "Failed to fork for background mode." << std::endl;
      return 1;
    }
    if (pid > 0) {
      // Parent process exits, child continues in background
      return 0;
    }
    // Child process becomes session leader, detaches from terminal
    setsid();
    // Redirect stdio to /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
  }

  if (initializeAudioEngine() != MA_SUCCESS) {
    if (!silent) std::cerr << "Failed to initialize audio engine" << std::endl;
    return 1;
  }

  std::string configPath = soundpackPath + "/config.json";
  std::string detectErr;
  PackVersion version = detectPackVersion(configPath, detectErr);
  if (version == PackVersion::Invalid) {
    if (!silent) std::cerr << detectErr << std::endl;
    return 1;
  }

  std::string devicePath;

  if (!targetDeviceName.empty()) {
    devicePath = getDevicePathByName(targetDeviceName);
    if (devicePath.empty()) {
      if (!silent)
        std::cerr << "Device with name '" << targetDeviceName << "' not found."
                  << std::endl;
      return 1;
    }
  } else {
    devicePath = getInputDevicePath(configDir);

    if (devicePath.empty()) {
      if (!silent) std::cout << "No device found. Prompting user." << std::endl;
      saveInputDevice(configDir);
      devicePath = getInputDevicePath(configDir);
    }
  }

  if (version == PackVersion::V2) {
    V2Pack pack;
    std::string err;
    if (!loadV2Pack(configPath, pack, err)) {
      if (!silent) std::cerr << err << std::endl;
      return 1;
    }
    // CLI -v wins, otherwise the pack's recommended_volume is the base volume.
    float volume = clampVolume(volumePassed ? cliVolume : pack.recommendedVolume);

    std::unordered_map<int, KeySound> keySounds;
    std::unordered_map<int, std::string> fallbackFiles;
    if (!loadV2Clips(pack, soundpackPath, keySounds, fallbackFiles, err)) {
      if (!silent) std::cerr << err << std::endl;
      return 1;
    }

    if (!silent) {
      std::cout << "Soundpack: " << pack.name << " by " << pack.author << " (V2, "
                << pack.soundpackType << ", " << keySounds.size() << " keys";
      if (!fallbackFiles.empty()) std::cout << ", " << fallbackFiles.size() << " files";
      std::cout << ")" << std::endl;
      if (pack.randomPitch) std::cout << "Random pitch enabled." << std::endl;
    }
    runMainLoopV2(devicePath, keySounds, fallbackFiles, volume, pack.randomPitch);
  } else {
    if (!silent) std::cout << "Soundpack: " << soundpackPath << " (V1)" << std::endl;
    float volume = clampVolume(volumePassed ? cliVolume : 1.0f);
    std::unordered_map<int, std::string> keySoundMap = loadKeySoundMappings(configPath);
    ensureV1FilesPlayable(configPath, soundpackPath, keySoundMap);
    runMainLoop(devicePath, keySoundMap, volume, soundpackPath);
  }

  ma_engine_uninit(&engine);
  return 0;
}
