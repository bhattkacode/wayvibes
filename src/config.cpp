#include <fstream>
#include <iostream>
#include <linux/input-event-codes.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

// Soundpack config.json files shipped by Mechvibes key their "defines" with Electron/DOM
// keycodes, which only overlap with Linux evdev codes for the base block (letters,
// digits, punctuation). Arrow keys, nav cluster and some numpad keys use entirely
// different numbers. Translate those into Linux codes so the ev.code lookup in the read
// loop hits the right sound.
int remapMechToLinuxKey(int mechCode) {
  switch (mechCode) {
  case 57416:
    return KEY_UP;
  case 57424:
    return KEY_DOWN;
  case 57419:
    return KEY_LEFT;
  case 57421:
    return KEY_RIGHT;
  case 61000: // win32 variants
    return KEY_UP;
  case 61008:
    return KEY_DOWN;
  case 61003:
    return KEY_LEFT;
  case 61005:
    return KEY_RIGHT;
  case 3655: // Home
    return KEY_HOME;
  case 60999:
    return KEY_HOME;
  case 3665: // End
    return KEY_END;
  case 61007:
    return KEY_END;
  case 3657: // PgUp
    return KEY_PAGEUP;
  case 61001:
    return KEY_PAGEUP;
  case 3663: // PgDn
    return KEY_PAGEDOWN;
  case 61009:
    return KEY_PAGEDOWN;
  case 3666: // Insert
    return KEY_INSERT;
  case 61010:
    return KEY_INSERT;
  case 3667: // Delete
    return KEY_DELETE;
  case 61011:
    return KEY_DELETE;
  case 3612: // Numpad Enter
    return KEY_KPENTER;
  case 3637: // Numpad /
    return KEY_KPSLASH;
  case 3597: // Numpad =
    return KEY_KPEQUAL;
  case 3639: // PrtSc
    return KEY_SYSRQ;
  case 3653: // Pause
    return KEY_PAUSE;
  case 91: // F13-F15
    return KEY_F13;
  case 92:
    return KEY_F14;
  case 93:
    return KEY_F15;
  case 3613: // right ctrl
    return KEY_RIGHTCTRL;
  case 3640: // right alt
    return KEY_RIGHTALT;
  case 3675:
    return KEY_LEFTMETA;
  case 3676:
    return KEY_RIGHTMETA;
  case 3677: // Menu
    return KEY_MENU;
  default:
    return mechCode;
  }
}

std::unordered_map<int, std::string> loadKeySoundMappings(const std::string &configPath) {
  std::unordered_map<int, std::string> keySoundMap;

  std::ifstream configFile(configPath);
  if (!configFile.is_open()) {
    std::cerr << "Could not open config.json file! Is the soundpack path correct?"
              << std::endl;
    exit(1);
    return keySoundMap;
  }

  try {
    json configJson;
    configFile >> configJson;

    if (configJson.contains("defines")) {
      for (auto &[key, value] : configJson["defines"].items()) {
        int keyCode = std::stoi(key);
        if (!value.is_null()) {
          std::string soundFile = value.get<std::string>();
          keySoundMap[remapMechToLinuxKey(keyCode)] = soundFile;
        }
      }
    }
  } catch (json::exception &e) {
    std::cerr << "Error parsing config.json: " << e.what() << std::endl;
  }

  return keySoundMap;
}
