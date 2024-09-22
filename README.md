# Wayvibes

Wayvibes is a Wayland-native C++ command-line interface (CLI) that plays mechanical keyboard sounds (or custom sounds) globally on keypresses. It leverages `libevdev` to capture keypress events and `miniaudio` to play sound effects.

## Currently Work in Progress

### Compiling

#### Prerequisites:

- `libevdev`

To compile the project, use the following command: 

```bash
g++ -o main main.cpp -levdev
```

### Usage

Run the application with the following command: 

```bash
./main <soundpack_path> -v <volume(1.0-10.0)>
```

**Example:** 

```bash
./main ./akko_lavender_purples/ -v 3
```

### Default Settings:

- **Soundpack Path:** `./`
- **Volume:** `2`

To specify your input device again, run: 

```bash
./main --prompt
```

This will store the device path in `$XDG_CONFIG_HOME/wayvibes/input_device_path`.

### Get Soundpacks

Wayvibes is compatible with the Mechvibes soundpack format. You can find soundpacks from the following sources:

- Mechvibes Soundpacks
- Discord Community

### Why Wayvibes?

Unlike mechvibes and rustyvibes, which have issues running on Wayland, Wayvibes aims to provide a seamless experience for users on this platform.
