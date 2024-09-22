# Wayvibes

Wayvibes is a Wayland-native CLI that plays mechanical keyboard sounds (or custom sounds) globally on keypresses. It leverages `libevdev` to capture keypress events and [miniaudio](https://miniaud.io) to play sound effects.

## Currently Work in Progress

### Compiling

#### Prerequisites:

- `libevdev`
- `nlohmann-json`

To compile the project, use the following command: 

```bash
g++ -o main main.cpp -levdev
```

### Usage

## First Steps
Add user to the `input` group by the following command:

```bash
sudo usermod -a -G input <your_username>
```

Then **REBOOT** or **Login again**.

Run the application with the following command: 

```bash
./main <soundpack_path> -v <volume(0.0-10.0)>
```

**Example:** 

```bash
./main ./akko_lavender_purples/ -v 3
```

## WARNING:
**Do not run the program with root privileges as it will monopolize the audio device until reboot!**

### Default Settings:

- **Soundpack Path:** `./`
- **Volume:** `1`

It will prompt you for your input device on the first run, and store it in `$XDG_CONFIG_HOME/wayvibes/input_device_path`.

The input device will usually be `HID ... keyboard` or `USB ... keyboard`. If you are using any key remapper, you have to select its virtual keyboard device.(Eg: `keyd virtual keyboard`)

**To specify your input device again, run:**

```bash
./main --prompt
```


### Get Soundpacks

Wayvibes is compatible with the Mechvibes soundpack format. You can find soundpacks from the following sources:

- [Mechvibes Soundpacks](https://docs.google.com/spreadsheets/d/1PimUN_Qn3CWqfn-93YdVW8OWy8nzpz3w3me41S8S494)
- [Discord Community](https://discord.com/invite/MMVrhWxa4w)

### Why Wayvibes?

Unlike [mechvibes](https://mechvibes.com) and [rustyvibes](https://github.com/KunalBagaria/rustyvibes), which have [issues](https://github.com/KunalBagaria/rustyvibes/issues/23) running on Wayland, Wayvibes aims to provide a seamless integration with wayland.
