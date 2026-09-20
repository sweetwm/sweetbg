sweetbg(1)

# NAME

sweetbg - control the sweetbg wallpaper daemon

# SYNOPSIS

*sweetbg* <command> [arguments]

# DESCRIPTION

*sweetbg* sends one command to a running *sweetbgd*(1) daemon and exits.

# COMMANDS

*img* <path>
	Set the default wallpaper on every output. JPEG, PNG, and WebP files are
	supported. If <path> is a directory, Sweetbg picks a random supported
	image from it. This clears per-output image overrides.

*img* <name>=<path> [<name>=<path> ...]
	Set wallpapers for named outputs. Output names come from *query*. A bare
	<path> may be included to also set the default wallpaper.

*img* <path> *--output* <name>
	Set the wallpaper for one output.

	Use *--persist* with any *img* form to save the selected image to the
	configuration file.

*set fit* <mode> [*--output* <name>]
	Set the fit mode to _cover_, _contain_, _center_, _tile_, or _span_. Use
	*--output* to change one output. _span_ applies to the whole layout and
	cannot be set per output. Setting the default clears per-output runtime fit
	overrides.

*set color* <#rrggbb|auto>
	Set the background color. It is visible when no image is set and behind
	_contain_ or _center_ images. _auto_ uses a dominant color from each image.

	Use *--persist* with either *set* command to save the change.

*clear* [*--image*] [*--fit*] [*--output* <name>] [*--blank*]
	Clear runtime wallpaper state. With no options, clear all images and show
	the background color.

	*--output* limits the change to one output. Add *--blank* to keep that
	output blank even when a default image is set. *--fit* clears fit settings;
	without *--output*, the default returns to _cover_. *--image* and *--fit*
	may be used together. *--persist* also removes the matching saved settings.

*query*
	Show the default wallpaper, fit mode, and connected outputs.

*query* *--json*
	Show the same state as JSON, including dominant image colors.

*doctor*
	Check the session, configuration, socket, and daemon connection.

*reload*
	Reread the configuration file and repaint the outputs.

*stop*
	Stop the daemon.

# OPTIONS

*-o*, *--output* <name>
	Limit a supported command to one output.

*-p*, *--persist*
	Save an *img*, *set*, or *clear* change to the configuration file.

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# EXIT STATUS

Returns 0 on success, 1 on a daemon or command error, and 2 for invalid usage.

# SEE ALSO

*sweetbgd*(1), *sweetbg*(5)
