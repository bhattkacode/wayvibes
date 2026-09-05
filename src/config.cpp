#include "config.h"
#include <algorithm>
#include <cctype>
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
  case 3663: // End
    return KEY_END;
  case 61007:
    return KEY_END;
  case 3657: // PgUp
    return KEY_PAGEUP;
  case 61001:
    return KEY_PAGEUP;
  case 3665: // PgDn
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

// --- V2 support ---

static std::string stripDotSlash(const std::string &p) {
  if (p.rfind("./", 0) == 0) return p.substr(2);
  return p;
}

static std::string lowerStr(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

static bool startsWith(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

// W3C KeyboardEvent.code -> Linux evdev. Built once, read-only afterwards.
static const std::unordered_map<std::string, int> &w3cTable() {
  static const std::unordered_map<std::string, int> table = {
      {"KeyA", KEY_A}, {"KeyB", KEY_B}, {"KeyC", KEY_C}, {"KeyD", KEY_D},
      {"KeyE", KEY_E}, {"KeyF", KEY_F}, {"KeyG", KEY_G}, {"KeyH", KEY_H},
      {"KeyI", KEY_I}, {"KeyJ", KEY_J}, {"KeyK", KEY_K}, {"KeyL", KEY_L},
      {"KeyM", KEY_M}, {"KeyN", KEY_N}, {"KeyO", KEY_O}, {"KeyP", KEY_P},
      {"KeyQ", KEY_Q}, {"KeyR", KEY_R}, {"KeyS", KEY_S}, {"KeyT", KEY_T},
      {"KeyU", KEY_U}, {"KeyV", KEY_V}, {"KeyW", KEY_W}, {"KeyX", KEY_X},
      {"KeyY", KEY_Y}, {"KeyZ", KEY_Z},
      {"Digit0", KEY_0}, {"Digit1", KEY_1}, {"Digit2", KEY_2}, {"Digit3", KEY_3},
      {"Digit4", KEY_4}, {"Digit5", KEY_5}, {"Digit6", KEY_6}, {"Digit7", KEY_7},
      {"Digit8", KEY_8}, {"Digit9", KEY_9},
      {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
      {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
      {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
      {"F13", KEY_F13}, {"F14", KEY_F14}, {"F15", KEY_F15},
      {"Enter", KEY_ENTER}, {"Escape", KEY_ESC}, {"Backspace", KEY_BACKSPACE},
      {"Tab", KEY_TAB}, {"Space", KEY_SPACE}, {"CapsLock", KEY_CAPSLOCK},
      {"NumLock", KEY_NUMLOCK},
      {"Minus", KEY_MINUS}, {"Equal", KEY_EQUAL}, {"BracketLeft", KEY_LEFTBRACE},
      {"BracketRight", KEY_RIGHTBRACE}, {"Backslash", KEY_BACKSLASH},
      {"Semicolon", KEY_SEMICOLON}, {"Quote", KEY_APOSTROPHE},
      {"Backquote", KEY_GRAVE}, {"Comma", KEY_COMMA}, {"Period", KEY_DOT},
      {"Slash", KEY_SLASH},
      {"ShiftLeft", KEY_LEFTSHIFT}, {"ShiftRight", KEY_RIGHTSHIFT},
      {"ControlLeft", KEY_LEFTCTRL}, {"ControlRight", KEY_RIGHTCTRL},
      {"AltLeft", KEY_LEFTALT}, {"AltRight", KEY_RIGHTALT},
      {"MetaLeft", KEY_LEFTMETA}, {"MetaRight", KEY_RIGHTMETA},
      {"OSLeft", KEY_LEFTMETA}, {"OSRight", KEY_RIGHTMETA},
      {"ContextMenu", KEY_MENU},
      {"ArrowUp", KEY_UP}, {"ArrowDown", KEY_DOWN}, {"ArrowLeft", KEY_LEFT},
      {"ArrowRight", KEY_RIGHT},
      {"Home", KEY_HOME}, {"End", KEY_END}, {"PageUp", KEY_PAGEUP},
      {"PageDown", KEY_PAGEDOWN}, {"Insert", KEY_INSERT}, {"Delete", KEY_DELETE},
      {"PrintScreen", KEY_SYSRQ}, {"ScrollLock", KEY_SCROLLLOCK}, {"Pause", KEY_PAUSE},
      {"Numpad0", KEY_KP0}, {"Numpad1", KEY_KP1}, {"Numpad2", KEY_KP2},
      {"Numpad3", KEY_KP3}, {"Numpad4", KEY_KP4}, {"Numpad5", KEY_KP5},
      {"Numpad6", KEY_KP6}, {"Numpad7", KEY_KP7}, {"Numpad8", KEY_KP8},
      {"Numpad9", KEY_KP9}, {"NumpadAdd", KEY_KPPLUS},
      {"NumpadSubtract", KEY_KPMINUS}, {"NumpadMultiply", KEY_KPASTERISK},
      {"NumpadDivide", KEY_KPSLASH}, {"NumpadDecimal", KEY_KPDOT},
      {"NumpadEnter", KEY_KPENTER},
      {"MouseLeft", BTN_LEFT}, {"MouseRight", BTN_RIGHT},
      {"MouseMiddle", BTN_MIDDLE}, {"Button4", BTN_SIDE}, {"Mouse4", BTN_SIDE},
      {"Button5", BTN_EXTRA}, {"Mouse5", BTN_EXTRA},
  };
  return table;
}

int w3cToEvdev(const std::string &code) {
  // Wheel is EV_REL on evdev, not EV_KEY, so there is nothing to map it to.
  if (startsWith(code, "Wheel") || startsWith(code, "MouseWheel")) return -2;
  auto it = w3cTable().find(code);
  if (it == w3cTable().end()) return -1;
  return it->second;
}

static bool readJsonFile(const std::string &path, json &out, std::string &err) {
  std::ifstream f(path);
  if (!f.is_open()) {
    err = "Could not open config.json file! Is the soundpack path correct?";
    return false;
  }
  try {
    f >> out;
  } catch (json::exception &e) {
    err = std::string("Invalid JSON in config.json: ") + e.what();
    return false;
  }
  return true;
}

PackVersion detectPackVersion(const std::string &configPath, std::string &err) {
  json configJson;
  if (!readJsonFile(configPath, configJson, err)) return PackVersion::Invalid;

  bool hasV2 = (configJson.contains("definitions") && configJson["definitions"].is_object()) ||
               (configJson.contains("defs") && configJson["defs"].is_object());
  bool hasV1 =
      configJson.contains("defines") && configJson["defines"].is_object();

  if (hasV2 && hasV1) {
    std::cerr << "Warning: config has both defines and definitions, preferring V2."
              << std::endl;
    return PackVersion::V2;
  }
  if (hasV2) return PackVersion::V2;
  if (hasV1) return PackVersion::V1;
  err = "No defines or definitions found in config.json. Not a valid soundpack.";
  return PackVersion::Invalid;
}

// config_version accepts 2 or "2". Anything higher means the pack needs a newer wayvibes.
static bool checkConfigVersion(const json &configJson, std::string &err) {
  if (!configJson.contains("config_version")) return true; // assume 2 by structure
  int version = 2;
  try {
    if (configJson["config_version"].is_number()) {
      version = static_cast<int>(configJson["config_version"].get<double>());
    } else if (configJson["config_version"].is_string()) {
      version = std::stoi(configJson["config_version"].get<std::string>());
    } else {
      std::cerr << "Warning: unreadable config_version, assuming 2." << std::endl;
      return true;
    }
  } catch (...) {
    std::cerr << "Warning: unreadable config_version, assuming 2." << std::endl;
    return true;
  }
  if (version > 2) {
    err = "This soundpack requires a newer version of wayvibes (config_version " +
          std::to_string(version) + ").";
    return false;
  }
  return true;
}

bool loadV2Pack(const std::string &configPath, V2Pack &out, std::string &err) {
  out = V2Pack(); // callers may reuse the struct, never leak stale fields
  json configJson;
  if (!readJsonFile(configPath, configJson, err)) return false;
  if (!checkConfigVersion(configJson, err)) return false;

  if (!configJson.contains("name") || !configJson["name"].is_string() ||
      configJson["name"].get<std::string>().empty()) {
    err = "Missing required V2 field: name.";
    return false;
  }
  out.name = configJson["name"].get<std::string>();

  if (configJson.contains("author") && configJson["author"].is_string() &&
      !configJson["author"].get<std::string>().empty()) {
    out.author = configJson["author"].get<std::string>();
  } else if (configJson.contains("m_author") && configJson["m_author"].is_string() &&
             !configJson["m_author"].get<std::string>().empty()) {
    out.author = configJson["m_author"].get<std::string>();
  } else {
    err = "Missing required V2 field: author.";
    return false;
  }

  const json *defs = nullptr;
  if (configJson.contains("definitions") && configJson["definitions"].is_object()) {
    defs = &configJson["definitions"];
  } else if (configJson.contains("defs") && configJson["defs"].is_object()) {
    defs = &configJson["defs"];
  }
  if (defs == nullptr || defs->empty()) {
    err = "Missing required V2 field: definitions (or defs).";
    return false;
  }

  if (configJson.contains("definition_method") &&
      configJson["definition_method"].is_string()) {
    out.definitionMethod = lowerStr(configJson["definition_method"].get<std::string>());
  }
  bool isMulti = (out.definitionMethod == "multi");
  if (isMulti) {
    std::cerr << "Warning: definition_method multi is not supported, using single-file "
                 "fallback for per-key audio files."
              << std::endl;
  }

  if (configJson.contains("audio_file") && configJson["audio_file"].is_string()) {
    out.audioFile = stripDotSlash(configJson["audio_file"].get<std::string>());
  }
  if (!isMulti && out.audioFile.empty()) {
    err = "Missing audio_file for single-method V2 pack.";
    return false;
  }

  if (configJson.contains("options") && configJson["options"].is_object()) {
    const json &opts = configJson["options"];
    if (opts.contains("recommended_volume")) {
      if (opts["recommended_volume"].is_number()) {
        out.recommendedVolume = opts["recommended_volume"].get<float>();
        if (out.recommendedVolume < 0.0f || out.recommendedVolume > 2.0f) {
          std::cerr << "Warning: recommended_volume out of range 0.0-2.0, clamping."
                    << std::endl;
          out.recommendedVolume = std::max(0.0f, std::min(2.0f, out.recommendedVolume));
        }
      } else {
        std::cerr << "Warning: recommended_volume is not a number, using 1.0."
                  << std::endl;
      }
    }
    if (opts.contains("random_pitch")) {
      if (opts["random_pitch"].is_boolean()) {
        out.randomPitch = opts["random_pitch"].get<bool>();
      } else {
        std::cerr << "Warning: random_pitch is not a boolean, using false."
                  << std::endl;
      }
    }
  }

  // Pack type: explicit soundpack_type wins, else auto-detect from key names.
  if (configJson.contains("soundpack_type") &&
      configJson["soundpack_type"].is_string()) {
    std::string t = lowerStr(configJson["soundpack_type"].get<std::string>());
    if (t == "mouse") {
      out.soundpackType = "Mouse";
      out.isMousePack = true;
    } else if (t == "keyboard") {
      out.soundpackType = "Keyboard";
      out.isMousePack = false;
    } else {
      std::cerr << "Warning: unknown soundpack_type, auto-detecting." << std::endl;
    }
  }
  if (out.soundpackType.empty()) {
    for (auto &[key, _] : defs->items()) {
      // Wheel keys are unsupported on evdev, so they don't vote on pack type.
      if (startsWith(key, "Mouse") || startsWith(key, "Button")) {
        out.isMousePack = true;
        break;
      }
    }
    out.soundpackType = out.isMousePack ? "Mouse" : "Keyboard";
  }

  // Parse each definition. The upstream doc claims extra timings are random
  // variants, but the DX engine treats timing[0] as keydown and timing[1] as
  // keyup, so that is what we do. Beyond 2, the rest are ignored.
  for (auto &[key, value] : defs->items()) {
    int evdev = w3cToEvdev(key);
    if (evdev == -2) {
      std::cerr << "Warning: wheel timings ignored on evdev (" << key << ")."
                << std::endl;
      continue;
    }
    if (evdev == -1) {
      std::cerr << "Warning: unknown key name '" << key << "', skipping." << std::endl;
      continue;
    }

    if (isMulti && value.is_object() && value.contains("audio_file") &&
        value["audio_file"].is_string()) {
      out.perKeyFiles[evdev] = stripDotSlash(value["audio_file"].get<std::string>());
    }

    if (!value.is_object() || !value.contains("timing") ||
        !value["timing"].is_array()) {
      err = "Invalid definitions entry for '" + key + "': expected timing array.";
      return false;
    }
    const json &timing = value["timing"];
    if (timing.empty()) {
      std::cerr << "Warning: empty timing for '" << key << "', skipping." << std::endl;
      continue;
    }
    if (timing.size() > 2) {
      std::cerr << "Warning: '" << key << "' has " << timing.size()
                << " timings, using press + release and ignoring the rest." << std::endl;
    }

    std::vector<V2Timing> windows;
    size_t count = std::min<size_t>(timing.size(), 2);
    for (size_t i = 0; i < count; i++) {
      const json &pair = timing[i];
      if (!pair.is_array() || pair.size() != 2 || !pair[0].is_number() ||
          !pair[1].is_number()) {
        err = "Invalid timing array for '" + key + "[" + std::to_string(i) +
              "]': expected [start, end].";
        return false;
      }
      V2Timing w{pair[0].get<double>(), pair[1].get<double>()};
      if (w.startMs < 0.0 || w.endMs <= w.startMs) {
        std::cerr << "Warning: invalid timing [" << w.startMs << ", " << w.endMs
                  << "] for '" << key << "', skipping." << std::endl;
        continue;
      }
      windows.push_back(w);
    }
    if (!windows.empty()) out.timings[evdev] = std::move(windows);
  }

  if (out.timings.empty() && out.perKeyFiles.empty()) {
    err = "No usable key mappings in definitions (all keys unknown or invalid).";
    return false;
  }
  return true;
}
