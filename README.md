# Wayvibes
A wayland native C++ CLI which plays mechanical keyboard sounds (or custom sounds) on keypresses.

## Currently Work In Progress

# Compiling

## Prerequisites:
- libevdev

`g++ -o main main.cpp -levdev`

# Usage
`./main <soundpack_path> -v <volume(1.0-10.0)>`

Example: `./main ./akko_lavender_purples/ -v 3`

Default soundpack path: `./`

Default volume: `2`

For specifying your input device again, run `./main --prompt`

It will be stored in $XDG_CONFIG_HOME/wayvibes/input_device_path
