sweetbg(5)

# NAME

sweetbg - configuration file for the sweetbg wallpaper daemon

# DESCRIPTION

*sweetbgd*(1) reads _$XDG_CONFIG_HOME/sweetbg/config.toml_, falling back to
_~/.config/sweetbg/config.toml_. The file is read at startup and when
*sweetbg reload* is run. There is no automatic hot reload.

A missing file uses safe defaults. A malformed file is reported with its path
and line number, and the daemon continues with defaults rather than failing to
start. Runtime *sweetbg img* and *sweetbg set* changes are not written back to
this file unless *--persist* is given (see *sweetbg*(1)), which rewrites only the
affected setting and preserves other keys, comments, and sections.
Runtime *sweetbg clear --persist* removes the matching saved keys while preserving
the rest of the file.

The format is a small subset of TOML: top-level _key = "value"_ entries with
double-quoted string values, _#_ comments (including trailing comments), and
_[output.NAME]_ sections. Unknown keys, unquoted values, malformed values, and
unknown sections are errors.

# KEYS

These top-level keys set the defaults applied to every output:

*image*
	Absolute path to the default wallpaper (JPEG, PNG, or WebP). When unset,
	the placeholder color is shown until an image is set with *sweetbg*(1).

	This must be a file. Unlike *sweetbg img*, which accepts a directory and
	picks a random image from it, the daemon never resolves a directory: one
	here is rejected as an unsupported image format and the placeholder color
	is shown. Use *sweetbg img* <dir> *--persist* to record a random choice,
	which writes the chosen file.

*color*
	Background color, as _"#rrggbb"_. Shown before an image is set, and behind
	the image where the _contain_ and _center_ fit modes do not cover the
	output. Defaults to _"#1e1e2e"_.

	Set it to _"auto"_ to derive the background from each image's dominant
	colour, so a _contain_ or _center_ letterbox matches the wallpaper instead
	of a fixed colour. An output with no image (placeholder, or blanked) has no
	image to sample, so it falls back to the last fixed _"#rrggbb"_ value.

*fit*
	Image fit mode. Defaults to _"cover"_.

	- _cover_: scale to fill the output, preserving aspect ratio, cropping the
	  overflow from the center.
	- _contain_: scale to fit entirely inside the output, preserving aspect
	  ratio, with the background color filling the letterbox margins.
	- _center_: place the image at its native size, centered; larger images are
	  cropped, smaller ones are padded with the background color.
	- _tile_: repeat the image at its native size across the output.
	- _span_: treat every output as one large surface and give each output its
	  slice of a single image, so one wallpaper stretches across the whole
	  monitor layout.

	The layout used by _span_ is the bounding box of all outputs, taken from the
	positions the compositor reports through the xdg-output protocol. Gaps
	between outputs, and monitors of different sizes or resolutions, are handled
	by sampling each output's own region of that box; the image is cover-fitted
	to the box first, so its aspect ratio is preserved. With a single output,
	_span_ is identical to _cover_.

	_span_ needs the compositor to support xdg-output
	(_zxdg_output_manager_v1_), which every wlr-layer-shell compositor in
	practice does. Without it the layout is unknown and each output falls back
	to _cover_.

# PER-OUTPUT SECTIONS

An _[output.NAME]_ section sets wallpaper choices for one output. NAME is the
output name shown by *sweetbg query* (for example _DP-1_ or _HDMI-A-1_). The
supported keys are *image* and *fit*, and each key overrides only that
top-level default. An output without a matching section uses the top-level
defaults.

*fit* cannot be set to _span_ in an _[output.NAME]_ section, because spanning is
defined across the whole layout rather than for one output. Setting it there is
a configuration error.

Set *image* to an empty string to intentionally show the background color on one
output even when a top-level default image exists. This is what
*sweetbg clear --output* <name> *--blank --persist* writes.

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

*sweetbg*(1), *sweetbgd*(1)
