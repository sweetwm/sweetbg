sweetbg(1)

# NAME

sweetbg - command-line client for the sweetbg wallpaper daemon

# SYNOPSIS

*sweetbg* <command> [arguments]

# DESCRIPTION

*sweetbg* sends a single request to a running *sweetbgd*(1) daemon and exits.
The daemon owns the Wayland connection and wallpaper state; the client only
parses arguments, sends one request, prints the response, and returns.

# COMMANDS

*img* <path>
	Set <path> (JPEG, PNG, or WebP) as the default wallpaper on every
	output, including outputs connected later, clearing any per-output image
	overrides. Paths are resolved to absolute paths before being sent.

*img* <name>=<path> [<name>=<path> ...]
	Set a different wallpaper per output in one command. Each <name> is an
	output name from *query* (for example _DP-1_); each is set as that
	output's override. A bare <path> may be mixed in to also set the
	default, e.g. _sweetbg img bg.jpg DP-1=left.jpg_.

*img* <path> *--output* <name>
	Set <path> on the single output <name> only (equivalent to the
	_<name>=<path>_ form).

	Pass *--persist* with any *img* form to also save the wallpaper to the
	config file (see *sweetbg*(5)) so it is restored on the next start.

*set fit* <mode>
	Change the fit mode at runtime to _cover_, _contain_, _center_, or
	_tile_, repainting the affected outputs. Without *--output*, this sets
	the default fit mode for every output and clears runtime per-output fit
	overrides.

*set fit* <mode> *--output* <name>
	Change the fit mode for the single output <name>. Other outputs keep
	using either their own override or the default fit mode.

*set color* <#rrggbb>
	Change the background color at runtime. It is shown before an image is
	set and behind the image in the _contain_ and _center_ fit modes.

	Either *set* form accepts *--persist* to also save the change to the
	config file.

*clear* [*--image*] [*--fit*] [*--output* <name>]
	Clear runtime wallpaper state. With no target options, this clears the
	default image and all per-output image overrides, returning configured
	outputs to the background color.

	With *--output* <name>, image clearing removes only that output's image
	override, so it falls back to the default image or background color.
	With *--output* <name> *--blank*, the output intentionally shows the
	background color even when a default image is set. *--fit* clears fit
	state instead: with *--output*, it removes that output's fit override;
	without *--output*, it resets the default fit to _cover_ and clears
	runtime per-output fit overrides. Pass both *--image* and *--fit* to
	clear both kinds of state.

	Pass *--persist* to also remove the matching saved keys from the config
	file.

*query*
	Print the current default wallpaper (image path or placeholder color) and
	the default fit mode, followed by one line per output with its name,
	resolution, scale, and any per-output override.

*query* *--json*
	Print the same daemon state as compact JSON for scripts. The object has a
	_default_ object with _image_, _color_, and _fit_, plus an _outputs_ array.
	Each output contains _name_, _width_, _height_, _scale_, _configured_,
	_image_, _imageOverride_, _blank_, _fit_, and _fitOverride_.

*doctor*
	Check the Sweetbg setup and print a compact diagnostic report. It verifies
	the session environment, config file syntax and configured image paths,
	the control socket, and daemon reachability. Warnings do not make the
	command fail; configuration errors, invalid runtime paths, socket
	ownership problems, or an unreachable daemon return a failure status.

*reload*
	Ask the running daemon to reread *sweetbg*(5) once and repaint outputs from
	the new configuration. This is an explicit command, not automatic hot
	reload. If the config is malformed or names an unreadable image, the daemon
	keeps the current wallpaper state and reports an error.

*stop*
	Ask the running daemon to exit.

# OPTIONS

*-o*, *--output* <name>
	Restrict *img*, *set fit*, or *clear* to a single output.

*-p*, *--persist*
	Save the applied *img*, *set*, or *clear* change to the config file.
	Updates or removes only the affected key (top-level default or the
	matching _[output.<name>]_ section) and preserves other keys, comments,
	and sections.

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# EXIT STATUS

Returns 0 on success. Returns 1 when no daemon is running or the daemon
reports an error, and 2 for invalid command-line usage.

# SEE ALSO

*sweetbgd*(1), *sweetbg*(5)
