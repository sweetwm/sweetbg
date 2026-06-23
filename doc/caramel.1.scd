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

*img* <path> [*--output* <name>]
	Set the wallpaper to the image at <path> (JPEG, PNG, or WebP). The path
	is resolved to an absolute path before being sent.

	Without *--output*, the image becomes the default for every output,
	including outputs connected later, and clears any per-output overrides.
	With *--output* <name>, only the named output is changed and the image
	becomes that output's override. Output names are those shown by *query*.

*query*
	Print the current default wallpaper (image path or placeholder color)
	followed by one line per output with its name, resolution, scale, and
	any per-output override.

*stop*
	Ask the running daemon to exit.

# OPTIONS

*-o*, *--output* <name>
	Restrict *img* to a single output.

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# EXIT STATUS

Returns 0 on success. Returns 1 when no daemon is running or the daemon
reports an error, and 2 for invalid command-line usage.

# SEE ALSO

*carameld*(1), *caramel*(5)
