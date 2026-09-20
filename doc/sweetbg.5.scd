sweetbg(5)

# NAME

sweetbg - configuration file for the sweetbg wallpaper daemon

# DESCRIPTION

*sweetbgd*(1) reads _$XDG_CONFIG_HOME/sweetbg/config.toml_, or
_~/.config/sweetbg/config.toml_ when _XDG_CONFIG_HOME_ is unset. It reads the
file at startup and when *sweetbg reload* is run.

A missing file uses defaults. Errors include the file and line number. Runtime
changes are temporary unless *--persist* is used; see *sweetbg*(1).

The file accepts double-quoted string values, _#_ comments, and
_[output.NAME]_ sections. Unknown or malformed settings are errors.

# KEYS

Top-level keys provide defaults for every output.

*image*
	Absolute path to a JPEG, PNG, or WebP wallpaper. It must name a file. To
	choose a random image from a directory, run *sweetbg img* <directory>
	*--persist*.

*color*
	Background color as _"#rrggbb"_, or _"auto"_ to use a dominant color from
	the image. The default is _"#1e1e2e"_.

*fit*
	Image fit mode. The default is _"cover"_.

	- _cover_: fill the output and crop overflow.
	- _contain_: fit the whole image and fill margins with the background.
	- _center_: center at native size, cropping or filling as needed.
	- _tile_: repeat at native size.
	- _span_: stretch one image across the complete output layout.

	_span_ requires compositor support for xdg-output. It falls back to
	_cover_ when the output layout is unavailable.

# PER-OUTPUT SECTIONS

An _[output.NAME]_ section overrides *image* or *fit* for one output. NAME is
shown by *sweetbg query*. Outputs without a matching section use the top-level
settings.

_span_ cannot be set per output. Set *image* to an empty string to show only
the background color on that output.

# EXAMPLE

```
image = "/home/me/Pictures/default.jpg"
color = "#1e1e2e"
fit = "cover"

[output.DP-1]
image = "/home/me/Pictures/left.jpg"
fit = "contain"
```

# SEE ALSO

*sweetbg*(1), *sweetbgd*(1)
