sweetbgd(1)

# NAME

sweetbgd - Wayland wallpaper daemon

# SYNOPSIS

*sweetbgd* [*-h*|*--help*] [*-V*|*--version*]

# DESCRIPTION

*sweetbgd* manages wallpapers on Wayland compositors that support
wlr-layer-shell. It reads *sweetbg*(5) and accepts commands from *sweetbg*(1).

Run it from your compositor or session startup, or enable the systemd user
service:

```
systemctl --user enable --now sweetbgd.service
```

The daemon runs in the foreground until stopped with *sweetbg stop*, *SIGINT*,
or *SIGTERM*. Only one instance can run at a time.

# OPTIONS

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# ENVIRONMENT

*WAYLAND_DISPLAY*
	Wayland display to connect to.

*XDG_RUNTIME_DIR*
	Directory for _sweetbg.sock_. Required.

*XDG_CONFIG_HOME*
	Base directory for the configuration file. Defaults to _~/.config_.

# FILES

_$XDG_CONFIG_HOME/sweetbg/config.toml_
	Configuration file.

_$XDG_RUNTIME_DIR/sweetbg.sock_
	Control socket.

# SIGNALS

*SIGINT*, *SIGTERM*
	Exit cleanly.

# SEE ALSO

*sweetbg*(1), *sweetbg*(5)
