# Sweetbg

Sweetbg is a small, lightweight and static Wayland wallpaper daemon

<a href="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" target="_blank">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" />
    <img alt="veila-preview2" src="https://github.com/user-attachments/assets/1af9d909-ac15-4d44-8280-b08ac0857bb2" />
  </picture>
</a>

## Install

Arch Linux AUR packages:

```sh
yay -S sweetbg-bin
# or
yay -S sweetbg-git
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

If your session does not export Wayland variables to systemd, import them from
your compositor startup:

```sh
systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP
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

# Set the background color (--persist or -p)
sweetbg set color "#1e1e2e" --persist

# Drop the wallpapers and go back to the background color
sweetbg clear

# Show the current wallpaper, fit mode, and connected outputs
sweetbg query

# Same, as JSON for scripts
sweetbg query --json

# Check the session, config, socket, and daemon when something is wrong
sweetbg doctor

# Reread the config after editing it
sweetbg reload

# Shut the daemon down
sweetbg stop
```

Output names come from `sweetbg query`. Fit modes are `cover`, `contain`,
`center`, `tile`, and `span`. See [`sweetbg(1)`](doc/sweetbg.1.scd) for the full
command reference.

Use `sweetbg doctor` to check the session environment, config file, socket and
daemon reachability when Sweetbg does not start or a client command cannot
connect.

### Random wallpapers

Pass a directory to `sweetbg img` to pick a random image from it. The choice is
made once, so redrawing an output never re-rolls it. There is no built-in timer;
rotate with a systemd user timer:

```sh
# rotate every 30 minutes
systemd-run --user --on-active=30m --on-unit-active=30m \
  sweetbg img ~/Pictures/Wallpapers/
```

### Spanning outputs

`span` stretches a single image across the whole monitor layout. It cannot be
set for a single output with `--output` or in an `[output.NAME]` config
section.

```sh
sweetbg set fit span
sweetbg img ~/Pictures/ultrawide.jpg
```

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

[Config example](config/example.toml)

Runtime changes are temporary unless you pass `--persist` or `-p`. There is no
config hot reload, run `sweetbg reload` after manual config edits. Every key is
documented in [`sweetbg(5)`](doc/sweetbg.5.scd).

## Documentation

The man pages are the full reference:

- [`sweetbg(1)`](doc/sweetbg.1.scd) - client commands, options, and fit modes
- [`sweetbgd(1)`](doc/sweetbgd.1.scd) - daemon and its environment
- [`sweetbg(5)`](doc/sweetbg.5.scd) - every config key

Once installed, read them with `man sweetbg`, `man sweetbgd`, and
`man 5 sweetbg`.

## Notes
- Supported image formats: JPEG, PNG, and WebP
- Sweetbg is Wayland-only and requires `zwlr_layer_shell_v1`
- Fractional scaling is sharper when the compositor supports `wp_viewporter`
  and `wp_fractional_scale_v1`
- The IPC socket is `$XDG_RUNTIME_DIR/sweetbg.sock`
