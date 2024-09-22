# Wayvibes

Wayvibes is a Wayland-native C++ command-line interface (CLI) that plays mechanical keyboard sounds (or custom sounds) globally on keypresses. It leverages `libevdev` to capture keypress events and [miniaudio](https://miniaud.io) to play sound effects.





## Currently Work in Progress

### Compiling

#### Prerequisites:

- `libevdev`

To compile the project, use the following command: 

```bash
g++ -o main main.cpp -levdev
```

### Usage

## Important
First, add user to the input group by the following command:

```bash
sudo usermod -a -G input <your_username>
```

Then **REBOOT** or **Logout**.

Run the application with the following command: 

```bash
./main <soundpack_path> -v <volume(1.0-10.0)>
```

**Example:** 

```bash
./main ./akko_lavender_purples/ -v 3
```

## WARNING:
**Do not run the program with root privileges as it will monopolize the audio device until reboot!**

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

- [Mechvibes Soundpacks](https://docs.google.com/spreadsheets/d/1PimUN_Qn3CWqfn-93YdVW8OWy8nzpz3w3me41S8S494)
- [Discord Community](https://discord.com/invite/MMVrhWxa4w)

### Why Wayvibes?

Unlike mechvibes and rustyvibes, which have [issues](https://github.com/KunalBagaria/rustyvibes/issues/23) running on Wayland, Wayvibes aims to provide a seamless experience for users on this platform.
