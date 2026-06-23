carameld(1)

# NAME

carameld - Wayland wallpaper daemon

# SYNOPSIS

*carameld* [*-h*|*--help*] [*-V*|*--version*]

# DESCRIPTION

*carameld* is a small, static wallpaper daemon for Wayland compositors that
support the wlr-layer-shell protocol. It creates one background layer surface
per output, paints a wallpaper, and then stays idle: an unchanged wallpaper
causes no periodic work and keeps almost no decoded image memory resident.

On startup it reads its configuration (see *caramel*(5)), connects to the
Wayland display, and serves requests from a *caramel*(1) client over a private
Unix-domain socket. Run it from a session startup script. It runs in the
foreground until it receives a *stop* request or is terminated.

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
	Directory holding the control socket _caramel.sock_. Required.

*XDG_CONFIG_HOME*
	Base directory for the config file; defaults to _~/.config_.

# FILES

_$XDG_CONFIG_HOME/caramel/config.toml_
	Configuration file. See *caramel*(5).

_$XDG_RUNTIME_DIR/caramel.sock_
	Control socket the client connects to.

# SIGNALS

*SIGINT*, *SIGTERM*
	Tear down surfaces and the socket, then exit cleanly.

# SEE ALSO

*caramel*(1), *caramel*(5)
