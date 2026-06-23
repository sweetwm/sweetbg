caramel(5)

# NAME

caramel - configuration file for the caramel wallpaper daemon

# DESCRIPTION

*carameld*(1) reads _$XDG_CONFIG_HOME/caramel/config.toml_, falling back to
_~/.config/caramel/config.toml_. The file is read once at startup; there is no
hot reload, so restart the daemon to apply edits.

A missing file uses safe defaults. A malformed file is reported with its path
and line number, and the daemon continues with defaults rather than failing to
start. A runtime *caramel img* change is not written back to this file.

The format is a small subset of TOML: top-level _key = "value"_ entries with
double-quoted string values, _#_ comments (including trailing comments), and
nothing else. Unknown keys, unquoted values, and malformed values are errors.

# KEYS

*image*
	Absolute path to the startup wallpaper (JPEG, PNG, or WebP). When unset,
	the placeholder color is shown until an image is set with *caramel*(1).

*color*
	Placeholder color shown before an image is set, as _"#rrggbb"_.
	Defaults to _"#1e1e2e"_.

*fit*
	Image fit mode. Only _"cover"_ is supported: the image is scaled to fill
	each output, preserving aspect ratio, with the overflow cropped from the
	center.

# EXAMPLE

```
image = "/home/me/Pictures/wallpaper.jpg"
color = "#1e1e2e"
fit   = "cover"
```

# SEE ALSO

*caramel*(1), *carameld*(1)
