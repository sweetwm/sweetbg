# Install the systemd user service so `systemctl --user enable sweetbgd` works.
# Optional: skipped when systemd is absent or -Dsystemd=disabled is passed.
systemd = dependency('systemd', required: get_option('systemd'))

if systemd.found()
	install_data(
		'sweetbgd.service',
		install_dir: systemd.get_variable(pkgconfig: 'systemduserunitdir'),
	)
endif
