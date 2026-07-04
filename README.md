# Sweetbg

Sweetbg is a small, lightweight and static Wayland wallpaper daemon

Binaries:

- `sweetbgd`: daemon
- `sweetbg`: command-line client

Supported image formats: JPEG, PNG, and WebP.

## Install

Arch Linux AUR packages:

```sh
yay -S sweetbg-bin
# or
yay -S sweetbg-git
```

Build from source:

```sh
meson setup build
ninja -C build
sudo ninja -C build install
```

Dependencies:

- `wayland-client`, `wayland-protocols`, `wayland-scanner`
- `libpng`, `libjpeg`, `libwebp`
- Meson, Ninja, and a C11 compiler
- `scdoc` for man pages
- `systemd` for the optional user service

## Usage

Start the daemon:

```sh
sweetbgd
```

Set and manage wallpapers:

```sh
sweetbg img ~/Pictures/wall.jpg
sweetbg img ~/Pictures/wall.jpg --persist
sweetbg img DP-1=~/left.jpg HDMI-A-1=~/right.jpg
sweetbg img ~/Pictures/side.jpg --output DP-1

sweetbg set fit cover
sweetbg set fit contain --output DP-1
sweetbg set color "#1e1e2e" --persist

sweetbg clear
sweetbg query
sweetbg query --json
sweetbg doctor
sweetbg stop
```

Output names come from `sweetbg query`. Fit modes are `cover`, `contain`,
`center`, and `tile`.

Use `sweetbg doctor` to check the session environment, config file, socket, and
daemon reachability when Sweetbg does not start or a client command cannot
connect.

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

[output.DP-1]
image = "/home/me/Pictures/left.jpg"
fit = "contain"
```

Runtime changes are temporary unless you pass `--persist` or `-p`. There is no config
hot reload; restart `sweetbgd` after manual config edits.

## Autostart

With systemd user services:

```sh
systemctl --user enable --now sweetbgd.service
```

If your session does not export Wayland variables to systemd, import them from
your compositor startup:

```sh
systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP
```

Without systemd, start the daemon from your compositor config:

```sh
# Sweets
sweets.exec_once("sweetbgd")
```

## Notes

- Sweetbg is Wayland-only and requires `zwlr_layer_shell_v1`.
- Fractional scaling is sharper when the compositor supports `wp_viewporter`
  and `wp_fractional_scale_v1`.
- The IPC socket is `$XDG_RUNTIME_DIR/sweetbg.sock`.
- See the man pages for full command details: `sweetbg(1)`, `sweetbgd(1)`,
  and `sweetbg(5)`.
