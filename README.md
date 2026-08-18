# Wayvibes

Wayvibes is a Wayland-native CLI made in C++ that plays mechanical keyboard sounds (or custom sounds) globally on keypresses. It utilizes `libevdev` to capture keypress events and [miniaudio](https://miniaud.io) to play sounds.

## Installing

### One liner install

```bash
curl -fsSl https://raw.githubusercontent.com/sahaj-b/wayvibes/main/install.sh | bash
```

### From AUR

Install [wayvibes-git](https://aur.archlinux.org/packages/wayvibes-git) from AUR (maintained by  [@justanoobcoder](https://www.github.com/justanoobcoder)).

```bash
#using yay
yay -S wayvibes-git
```

### NixOS

Add `wayvibes` url to your inputs:
```nix
inputs = {
  nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  wayvibes = {
    url = "github:sahaj-b/wayvibes";
    inputs.nixpkgs.follows = "nixpkgs";
  };
};
```

Install `wayvibes` and enable service using home-manager:
```nix
# home.nix
{inputs, ...}: {
  imports = [
    inputs.wayvibes.nixosModules.default
  ];

  services = {
    wayvibes = {
      enable = true;
      soundpack = "/home/youruser/wayvibes/soundpacks/cherrymx-red-abs";
      volume = 5;
    };
  };
}
```

### From Source

#### Prerequisites

Ensure the following dependencies are installed:

**Ubuntu/debian-based distros:**

- `libevdev-dev`
- `nlohmann-json*-dev`

Install them with:
`sudo apt install libevdev-dev nlohmann-json*-dev`

**Arch-based distros:**

- `libevdev`
- `nlohmann-json`

Install them with:
`sudo pacman -S libevdev nlohmann-json`

To install wayvibes, use the following commands:

```bash
git clone https://github.com/sahaj-b/wayvibes
cd wayvibes
make
sudo make install
```

## Uninstalling

```bash
cd ~/wayvibes
sudo make uninstall
```

## Usage

### Initial Setup

> [!NOTE]
> This step is already done if you used the one-liner install script.

1. Add user to the `input` group by the following command:

```bash
sudo usermod -a -G input <your_username>
```

2. **Reboot** or **Logout and Login** for the changes to take effect.

3. Run the application:

```
Usage: wayvibes [options] [soundpack_path]
Options:
  --device              Select input device
  --device-name <name>  Use input device by name
  -v <volume>           Set volume (0.0-10.0) (default: 1.0)
  --background, -bg     Run in background (detached from terminal)
  --help, -h            Show this help message;

Note: default soundpack path is `./`(current directory)

wayvibes <soundpack_path> -v <volume(0.0-10.0)>
```

> [!NOTE]
> Use `--background`/`-bg` if adding the command to a startup file like `.profile`

**Example:**

```bash
wayvibes ~/wayvibes/soundpacks/akko_lavender_purples/ -v 3
wayvibes ~/wayvibes/soundpacks/nk-cream/ -v 5 --background
```

#### Note

- Default **Soundpack Path:** `./`
- Default **Volume:** `1`

### Keyboard Device Configuration

Upon the first run, Wayvibes will prompt you to select your keyboard device if there are multiple available. This selection will be stored in:

`$XDG_CONFIG_HOME/wayvibes/input_device`

Typically, the input device will be something like `AT Translated Set 2 keyboard` or `USB Keyboard`. If you use a key remapper like `keyd`, select its virtual device (e.g., `keyd virtual keyboard`).

To reset and prompt for input device selection again, use:

```bash
wayvibes --device
```

If you use NixOS, you have to restart the service after changing the device.
```bash
systemctl --user restart wayvibes.service
```

> [!WARNING]
> **Do not run the program with sudo/root privileges as it will monopolize the audio device until reboot.**

## Get Soundpacks

Wayvibes is compatible with the Mechvibes soundpack format. So, You can find soundpacks from the following sources:

- [Mechvibes Soundpacks](https://docs.google.com/spreadsheets/d/1PimUN_Qn3CWqfn-93YdVW8OWy8nzpz3w3me41S8S494)
- [Discord Community](https://discord.com/invite/MMVrhWxa4w) (got akko_lavender_purples soundpack from here)

### Note

Some soundpacks with single audio file configuration won't work, use [this tool](https://github.com/KunalBagaria/packfixer-rustyvibes) to convert them into a compatible format

### Pre-converted Soundpacks

Ready-to-use soundpacks (already converted to wav format) are available in the [`soundpacks/`](./soundpacks/) directory of this repository. Just clone the repo and point wayvibes to the pack:

```bash
wayvibes ~/wayvibes/soundpacks/nk-cream/ -v 3
```

Available packs:

- akko_lavender_purples
- apex pro
- banana split lubed / stock
- boxjade
- cherrymx-black-abs / cherrymx-black-pbt
- cherrymx-blue-abs / cherrymx-blue-pbt
- cherrymx-brown-pbt
- cherrymx-red-abs / cherrymx-red-pbt
- Creams
- eg-crystal-purple
- eg-oreo
- kalih-box-white
- mx-speed-silver
- nk-cream
- Razer Green (Blackwidow Elite) - Akira
- topre-purple-hybrid-pbt

### Ogg files incompatibility

Wayvibes uses miniaudio to play sounds, which doesn't support all ogg files by default. So, you need to convert ogg files to wav/mp3 files using `ffmpeg` or `sox`, and change the extensions in the `config.json` file. Use this command for this:

Converting ogg files to wav using `ffmpeg` and change extensions in `config.json`:

```bash
cd <soundpack_path>
for f in *.ogg; do ffmpeg -i "$f" "${f%.ogg}.wav"; done && sed -i 's/\.ogg/\.wav/g' config.json
rm *.ogg # remove ogg files
```

## Why Wayvibes?

Unlike [mechvibes](https://mechvibes.com) and [rustyvibes](https://github.com/KunalBagaria/rustyvibes), which encounter [issues](https://github.com/KunalBagaria/rustyvibes/issues/23) on Wayland, Wayvibes aims to provide a seamless integration with wayland.

## Community Projects

- [wayvibes-tui](https://github.com/caml07/wayvibes-tui): A native Rust TUI for Wayvibes (Ratatui). Allows browsing soundpacks, selecting input and output keyboard/audio devices, and start/stop/restart Wayvibes from a nice interface.
