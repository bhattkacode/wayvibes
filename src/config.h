#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>

// Function to load the key-sound mappings from a JSON configuration file (V1 packs)
std::unordered_map<int, std::string> loadKeySoundMappings(const std::string &configPath);

// --- MechvibesDX V2 (config_version 2) support ---
// V2 packs map W3C key names (e.g. "KeyA", "Space", "MouseLeft") to [start_ms, end_ms]
// slices inside a single audio file. timing[0] plays on keydown, timing[1] on keyup.

enum class PackVersion { V1, V2, Invalid };

// A [start_ms, end_ms] window inside the pack's audio file.
struct V2Timing {
  double startMs = 0.0;
  double endMs = 0.0;
};

struct V2Pack {
  std::string name;
  std::string author;
  std::string audioFile; // as written in config.json (resolved against pack dir at load)
  std::string definitionMethod = "single"; // "single" or "multi"
  std::string soundpackType;               // "Keyboard" or "Mouse" (auto-detected if empty)
  float recommendedVolume = 1.0f;
  bool randomPitch = false;
  bool isMousePack = false;
  // evdev code -> timing windows. size 1 = press only, size >= 2 = press + release.
  std::unordered_map<int, std::vector<V2Timing>> timings;
  // evdev code -> per-key sound file. Only used for definition_method "multi" fallback.
  std::unordered_map<int, std::string> perKeyFiles;
};

// Peek at config.json and decide whether it is a V1 ("defines") or V2
// ("definitions"/"defs") pack. Returns Invalid and sets err on unreadable JSON or
// when neither key is present.
PackVersion detectPackVersion(const std::string &configPath, std::string &err);

// Parse a V2 config.json. Returns false and sets err on any fatal problem
// (missing fields, bad timings, config_version > 2, ...). Warnings go to stderr.
bool loadV2Pack(const std::string &configPath, V2Pack &out, std::string &err);

// Translate a W3C KeyboardEvent.code name ("KeyA", "Space", "MouseLeft", ...) to a
// Linux evdev code (KEY_A, KEY_SPACE, BTN_LEFT, ...). Returns -1 for unknown names
// and -2 for wheel names (EV_REL, unsupported).
int w3cToEvdev(const std::string &code);

#endif // CONFIG_H
