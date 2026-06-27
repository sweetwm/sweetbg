manju(5)

# NAME

manju - configuration file for the manju wallpaper daemon

# DESCRIPTION

*manjud*(1) reads _$XDG_CONFIG_HOME/manju/config.toml_, falling back to
_~/.config/manju/config.toml_. The file is read once at startup; there is no
hot reload, so restart the daemon to apply edits.

A missing file uses safe defaults. A malformed file is reported with its path
and line number, and the daemon continues with defaults rather than failing to
start. Runtime *manju img* and *manju set* changes are not written back to
this file unless *--persist* is given (see *manju*(1)), which rewrites only the
affected setting and preserves other keys, comments, and sections.

The format is a small subset of TOML: top-level _key = "value"_ entries with
double-quoted string values, _#_ comments (including trailing comments), and
_[output.NAME]_ sections. Unknown keys, unquoted values, malformed values, and
unknown sections are errors.

# KEYS

These top-level keys set the defaults applied to every output:

*image*
	Absolute path to the default wallpaper (JPEG, PNG, or WebP). When unset,
	the placeholder color is shown until an image is set with *manju*(1).

*color*
	Background color, as _"#rrggbb"_. Shown before an image is set, and behind
	the image where the _contain_ and _center_ fit modes do not cover the
	output. Defaults to _"#1e1e2e"_.

*fit*
	Image fit mode. Defaults to _"cover"_.

	- _cover_: scale to fill the output, preserving aspect ratio, cropping the
	  overflow from the center.
	- _contain_: scale to fit entirely inside the output, preserving aspect
	  ratio, with the background color filling the letterbox margins.
	- _center_: place the image at its native size, centered; larger images are
	  cropped, smaller ones are padded with the background color.
	- _tile_: repeat the image at its native size across the output.

# PER-OUTPUT SECTIONS

An _[output.NAME]_ section sets wallpaper choices for one output. NAME is the
output name shown by *manju query* (for example _DP-1_ or _HDMI-A-1_). The
supported keys are *image* and *fit*, and each key overrides only that
top-level default. An output without a matching section uses the top-level
defaults.

# EXAMPLE

```
image = "/home/me/Pictures/default.jpg"
color = "#1e1e2e"
fit   = "cover"

[output.DP-1]
image = "/home/me/Pictures/left.jpg"
fit = "contain"

[output.HDMI-A-1]
image = "/home/me/Pictures/tv.png"
```

# SEE ALSO

*manju*(1), *manjud*(1)
