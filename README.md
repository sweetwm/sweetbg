<h1 align=center>sweetbg</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/sweetwm/sweetbg?style=for-the-badge&labelColor=181825&color=a6e3a1)
![GitHub repo size](https://img.shields.io/github/repo-size/sweetwm/sweetbg?style=for-the-badge&labelColor=181825&color=d3bfe6)
![AUR Version](https://img.shields.io/aur/version/sweetbg-bin?style=for-the-badge&labelColor=181825&color=b4befe)
![GitHub Repo stars](https://img.shields.io/github/stars/sweetwm/sweetbg?style=for-the-badge&labelColor=181825&color=f9e2af)


Sweetbg is a small, lightweight and static Wayland wallpaper daemon

Runs on Linux and FreeBSD with a compositor that supports `wlr-layer-shell`

[< Wiki >](https://sweetwm.org/docs/sweetbg)


</div>

<a href="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" target="_blank">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" />
    <img alt="veila-preview2" src="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" />
  </picture>
</a>

---

## 🔥 Features

- Change wallpapers at runtime from the command line
- Per-monitor wallpapers, fit modes, and blur settings
- Cover, contain, center, tile and multi-monitor span modes
- JPEG, PNG, and WebP support
- Optional blur, processed once when applying the wallpaper
- Pick a random wallpaper from a directory
- Automatic background colors and dominant-color palettes for theming
- Fractional scaling and monitor hotplug support
- JSON status output for scripts and built-in setup diagnostics
- Runs on Linux and FreeBSD

## Install

### Arch Linux

On Arch Linux, install sweetbg from the AUR:

```sh
yay -S sweetbg-bin
# or
yay -S sweetbg-git
```

### NixOS

```sh
# Try without installing
nix run github:sweetwm/sweetbg -- --version
```

```nix
# flake.nix inputs
sweetbg.url = "github:sweetwm/sweetbg";

# configuration (pass inputs to your modules via specialArgs)
nixpkgs.overlays = [ inputs.sweetbg.overlays.default ];
environment.systemPackages = [ pkgs.sweetbg ];

# systemd user service
systemd.packages = [ pkgs.sweetbg ];
systemd.user.services.sweetbgd.wantedBy = [ "graphical-session.target" ];
```

Build from source:

```sh
meson setup build --buildtype=release
ninja -C build
sudo ninja -C build install
```

Dependencies:

- `wayland-client`, `wayland-protocols`, `wayland-scanner`
- `libpng`, `libjpeg`, `libwebp`
- Meson, Ninja, and a C11 compiler
- `scdoc` for man pages
- `systemd` for the optional user service

On FreeBSD, install the build dependencies first:

```sh
sudo pkg install meson ninja pkgconf wayland wayland-protocols png jpeg-turbo webp scdoc
```

Then build from source using the commands above. Start `sweetbgd` from your
compositor's startup configuration. The systemd service is optional and is
skipped when systemd is unavailable.

Binaries: `sweetbgd`: daemon, `sweetbg`: command-line client

## Usage

Start the daemon:

```sh
sweetbgd
```

With systemd user services:

```sh
systemctl --user enable --now sweetbgd.service
```

Set and manage wallpapers:

```sh
# Set the wallpaper on every output
sweetbg img ~/Pictures/wall.jpg

# Same, but also save it to the config so it comes back next start (--persist or -p)
sweetbg img ~/Pictures/wall.jpg --persist

# Pass a directory to pick a random image from it
sweetbg img ~/Pictures/Wallpapers/

# Give each output its own wallpaper
sweetbg img DP-1=~/left.jpg HDMI-A-1=~/right.jpg

# Set the wallpaper on one output only (--output or -o)
sweetbg img ~/Pictures/side.jpg --output DP-1

# Change how the image fills every output
sweetbg set fit cover

# Change the fit mode for one output only (--output or -o)
sweetbg set fit contain --output DP-1

# Blur the wallpaper (0 disables; --output and --persist are supported)
sweetbg set blur 24

# Set the background color (--persist or -p); "auto" derives it from the image
sweetbg set color "#1e1e2e" --persist

# Drop the wallpapers and go back to the background color
sweetbg clear

# Show the current wallpaper, fit mode, and connected outputs
sweetbg query

# Same, as JSON for scripts (includes dominant colors per output)
sweetbg query --json

# Check the session, config, socket, and daemon when something is wrong
sweetbg doctor

# Reread the config after editing it
sweetbg reload

# Shut the daemon down
sweetbg stop
```

See [`sweetbg(1)`](doc/sweetbg.1.scd) for the full command reference

Use `sweetbg doctor` to check the session environment, config file, socket and
daemon reachability when Sweetbg does not start or a client command cannot
connect

### Spanning outputs

`span` stretches a single image across the whole monitor layout. It cannot be
set for a single output with `--output` or in an `[output.NAME]` config
section

```sh
sweetbg set fit span
sweetbg img ~/Pictures/ultrawide.jpg
```

### Blur

Blur is optional and disabled by default. Set its strength from 0 to 100:

```sh
sweetbg set blur 24 --persist
sweetbg set blur 0 --output DP-1
sweetbg clear --blur --output DP-1
```

### Theming

`query --json` reports each output's dominant colors, so the current wallpaper
can drive a color scheme:

```sh
sweetbg query --json | jq -r '.outputs[0].colors[0]'
```

Relatedly, `color = "auto"` (or `sweetbg set color auto`) fills the
`contain`/`center` letterbox with the image's own dominant color

## Configuration

Sweetbg reads:

```text
$XDG_CONFIG_HOME/sweetbg/config.toml
~/.config/sweetbg/config.toml
```

Example:

```toml
image = "/home/me/Pictures/wallpaper.jpg"
color = "#1e1e2e"
fit = "cover"
blur = 0

[output.DP-1]
image = "/home/me/Pictures/left.jpg"
fit = "contain"
```

[Config example](config/example.toml)

Runtime changes are temporary unless you pass `--persist` or `-p`. There is no
config hot reload, run `sweetbg reload` after manual config edits. Every key is
documented in [`sweetbg(5)`](doc/sweetbg.5.scd).

## Documentation

The man pages are the full reference:

- [`sweetbg(1)`](doc/sweetbg.1.scd) - client commands, options, and fit modes
- [`sweetbgd(1)`](doc/sweetbgd.1.scd) - daemon and its environment
- [`sweetbg(5)`](doc/sweetbg.5.scd) - every config key

## Notes
- Supported image formats: JPEG, PNG, and WebP
- Sweetbg is Wayland-only and requires `zwlr_layer_shell_v1`
- Fractional scaling is sharper when the compositor supports `wp_viewporter`
  and `wp_fractional_scale_v1`
- The IPC socket is `$XDG_RUNTIME_DIR/sweetbg.sock`
