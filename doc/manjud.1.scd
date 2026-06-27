manjud(1)

# NAME

manjud - Wayland wallpaper daemon

# SYNOPSIS

*manjud* [*-h*|*--help*] [*-V*|*--version*]

# DESCRIPTION

*manjud* is a small, static wallpaper daemon for Wayland compositors that
support the wlr-layer-shell protocol. It creates one background layer surface
per output, paints a wallpaper, and then stays idle: an unchanged wallpaper
causes no periodic work and keeps almost no decoded image memory resident.

On startup it reads its configuration (see *manju*(5)), connects to the
Wayland display, and serves requests from a *manju*(1) client over a private
Unix-domain socket. Run it from a session startup script, your compositor's
*exec* directive, or the bundled systemd user service
(_systemctl --user enable --now manjud.service_). It runs in the foreground
until it receives a *stop* request or is terminated.

When the compositor supports the *wp_viewporter* and *wp_fractional_scale_v1*
protocols, *manjud* renders each wallpaper at the output's exact device
resolution and maps it down through a viewport, so wallpapers stay sharp on
fractionally scaled outputs (for example 1.5x). Without those protocols it falls
back to integer output scaling.

Only one daemon runs at a time; a second instance exits if a live socket
already exists.

# OPTIONS

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# ENVIRONMENT

*WAYLAND_DISPLAY*
	The Wayland display to connect to.

*XDG_RUNTIME_DIR*
	Directory holding the control socket _manju.sock_. Required.

*XDG_CONFIG_HOME*
	Base directory for the config file; defaults to _~/.config_.

# FILES

_$XDG_CONFIG_HOME/manju/config.toml_
	Configuration file. See *manju*(5).

_$XDG_RUNTIME_DIR/manju.sock_
	Control socket the client connects to.

_$prefix/lib/systemd/user/manjud.service_
	Optional systemd user service, installed when systemd is present.

# SIGNALS

*SIGINT*, *SIGTERM*
	Tear down surfaces and the socket, then exit cleanly.

# SEE ALSO

*manju*(1), *manju*(5)
