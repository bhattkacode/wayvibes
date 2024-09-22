#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem> // For filesystem operations
#include <fstream>
#include <iostream>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/stat.h> // For mkdir
#include <unistd.h>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

// Global engine instance
ma_engine engine;

float defaultVolume = 1.0f;

// Function to play sound asynchronously
void playSound(const char *soundFile, float volume) {
  ma_sound *sound = new ma_sound; // Allocate sound on the heap

  // Initialize the sound and attach it to the engine
  if (ma_sound_init_from_file(&engine, soundFile, MA_SOUND_FLAG_ASYNC, NULL,
                              NULL, sound) != MA_SUCCESS) {
    std::cerr << "Failed to initialize sound: " << soundFile << std::endl;
    delete sound; // Clean up allocated sound object if initialization fails
    return;
  }

  // Set the volume for the sound
  ma_sound_set_volume(sound, volume);

  // Play the sound asynchronously
  if (ma_sound_start(sound) != MA_SUCCESS) {
    std::cerr << "Failed to play sound: " << soundFile << std::endl;
    ma_sound_uninit(sound);
    delete sound; // Clean up allocated memory
  }

  // The sound will play asynchronously and we let it clean up automatically
}

// Function to load the key-sound mappings from config.json
std::unordered_map<int, std::string>
loadKeySoundMappings(const std::string &configPath) {
  std::unordered_map<int, std::string> keySoundMap;

  std::ifstream configFile(configPath);
  if (!configFile.is_open()) {
    std::cerr << "Could not open config.json file!" << std::endl;
    return keySoundMap;
  }

  try {
    json configJson;
    configFile >> configJson;

    // Parse the "defines" field in the JSON
    if (configJson.contains("defines")) {
      for (auto &[key, value] : configJson["defines"].items()) {
        int keyCode = std::stoi(key); // Convert string key to integer
        if (!value.is_null()) {
          std::string soundFile = value.get<std::string>();
          keySoundMap[keyCode] = soundFile;
        }
      }
    }
  } catch (json::exception &e) {
    std::cerr << "Error parsing config.json: " << e.what() << std::endl;
  }

  return keySoundMap;
}

// Function to list input devices and filter for keyboards
std::string findKeyboardDevice() {
  DIR *dir = opendir("/dev/input");
  if (!dir) {
    std::cerr << "Failed to open /dev/input directory" << std::endl;
    return "";
  }

  std::vector<std::string> devices;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "event", 5) == 0) {
      devices.push_back(entry->d_name);
    }
  }

  closedir(dir);

  if (devices.empty()) {
    std::cerr << "No input devices found!" << std::endl;
    return "";
  }

  // Display available keyboard devices
  std::cout << "Available keyboard input devices:" << std::endl;
  for (const auto &device : devices) {
    std::string devicePath = "/dev/input/" + device;
    struct libevdev *dev = nullptr;
    int fd = open(devicePath.c_str(), O_RDONLY);
    if (fd < 0) {
      std::cerr << "Error opening " << devicePath << std::endl;
      continue;
    }

    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
      std::cerr << "Failed to create evdev device for " << devicePath
                << std::endl;
      close(fd);
      continue;
    }

    // Check if the device is a keyboard
    if (libevdev_has_event_type(dev, EV_KEY)) {
      std::cout << device << ": " << libevdev_get_name(dev) << std::endl;
    }

    libevdev_free(dev);
    close(fd);
  }

  std::string selectedDevice;
  bool validChoice = false;

  // Loop until a valid choice is made
  while (!validChoice) {
    std::cout << "Select a keyboard input device (1-" << devices.size()
              << "): ";
    int choice;
    std::cin >> choice;

    if (choice >= 1 && choice <= devices.size()) {
      selectedDevice = devices[choice];
      validChoice = true;
    } else {
      std::cerr << "Invalid choice. Please try again." << std::endl;
    }
  }

  return selectedDevice; // Return the selected device
}

// Function to get the input device path
std::string getInputDevicePath(std::string &configDir) {
  const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
  configDir = (xdgConfigHome ? xdgConfigHome
                             : std::string(getenv("HOME")) + "/.config") +
              "/wayvibes";

  // Create the directory if it doesn't exist
  if (!std::filesystem::exists(configDir)) {
    std::filesystem::create_directories(configDir);
  }

  // Check for the input_device_path file
  std::string inputFilePath = configDir + "/input_device_path";
  std::ifstream inputFile(inputFilePath);

  if (inputFile.is_open()) {
    std::string devicePath;
    std::getline(inputFile, devicePath);
    inputFile.close();
    return devicePath; // Return the found device path
  }

  // If the file does not exist, return an empty string
  return "";
}

int main(int argc, char *argv[]) {
  // Default soundpack path and volume
  std::string soundpackPath = "./";
  float volume = defaultVolume;
  std::string configDir;   // Define configDir here
  bool promptUser = false; // Flag to check if user wants to be prompted again

  // Check for arguments
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--prompt") {
      promptUser = true; // Set flag if --prompt is provided
    } else if (std::string(argv[i]) == "-v" && (i + 1) < argc) {
      try {
        volume = std::stof(argv[i + 1]); // Convert to float
        i++;                             // Skip the next argument
      } catch (...) {
        std::cerr << "Invalid volume argument. Using default volume."
                  << std::endl;
      }
    } else if (argv[i][0] != '-') {
      soundpackPath = argv[i]; // Set the sound pack path
    }
  }

  // Clamp the volume
  volume = std::clamp(volume, 0.0f, 10.0f);

  // Initialize the audio engine
  if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
    std::cerr << "Failed to initialize audio engine" << std::endl;
    return 1;
  }

  // Load key-sound mappings from config.json
  std::cout << "Soundpack path: " << soundpackPath << std::endl;
  std::string configFilePath = soundpackPath + "/config.json";
  std::unordered_map<int, std::string> keySoundMap =
      loadKeySoundMappings(configFilePath);

  std::string devicePath;

  // Get the input device path
  while (true) {
    devicePath = getInputDevicePath(configDir);
    if (devicePath.empty() || promptUser) {
      std::cout << "No device found. Please select a keyboard input device."
                << std::endl;
      std::string selectedDevice = findKeyboardDevice();
      if (!selectedDevice.empty()) {
        // Write the selected device path to the file
        std::ofstream outputFile(configDir + "/input_device_path");
        outputFile << "/dev/input/" << selectedDevice;
        outputFile.close();
        devicePath = "/dev/input/" + selectedDevice;
      }
    }
    if (!devicePath.empty()) {
      break; // Exit the loop if a valid device path is found
    }
  }

  // Open the selected input device
  int fd = open(devicePath.c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "Error: Cannot open device " << devicePath << std::endl;
    return 1;
  }
  std::cout << "Using " << devicePath << std::endl;

  struct libevdev *dev = NULL;
  int rc = libevdev_new_from_fd(fd, &dev);
  if (rc < 0) {
    std::cerr << "Error: Failed to create evdev device" << std::endl;
    return 1;
  }

  while (true) {
    struct input_event ev;
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0) {
      if (ev.type == EV_KEY && ev.value == 1) { // Key press event
        // Check if the keycode exists in the mapping
        if (keySoundMap.find(ev.code) != keySoundMap.end()) {
          std::string soundFile =
              soundpackPath + "/" + keySoundMap[ev.code]; // Construct full path
          playSound(soundFile.c_str(),
                    volume); // Play the sound for the keycode
        } else {
          std::cerr << "No sound mapped for keycode: " << ev.code << std::endl;
        }
      }
    }
  }

  libevdev_free(dev);
  close(fd);

  // Clean up the audio engine when done
  ma_engine_uninit(&engine);
  return 0;
}
