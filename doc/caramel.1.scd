caramel(1)

# NAME

caramel - command-line client for the caramel wallpaper daemon

# SYNOPSIS

*caramel* <command> [arguments]

# DESCRIPTION

*caramel* sends a single request to a running *carameld*(1) daemon and exits.
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
	default, e.g. _caramel img bg.jpg DP-1=left.jpg_.

*img* <path> *--output* <name>
	Set <path> on the single output <name> only (equivalent to the
	_<name>=<path>_ form).

	Pass *--persist* with any *img* form to also save the wallpaper to the
	config file (see *caramel*(5)) so it is restored on the next start.

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

*query*
	Print the current default wallpaper (image path or placeholder color) and
	the default fit mode, followed by one line per output with its name,
	resolution, scale, and any per-output override.

*stop*
	Ask the running daemon to exit.

# OPTIONS

*-o*, *--output* <name>
	Restrict *img* or *set fit* to a single output.

*-p*, *--persist*
	Save the applied *img* or *set* change to the config file. Updates only
	the affected key (top-level default or the matching _[output.<name>]_
	section) and preserves other keys, comments, and sections.

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# EXIT STATUS

Returns 0 on success. Returns 1 when no daemon is running or the daemon
reports an error, and 2 for invalid command-line usage.

# SEE ALSO

*carameld*(1), *caramel*(5)
