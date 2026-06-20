project(
	'caramel',
	'c',
	version: '0.1.0',
	meson_version: '>= 0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	'-DCARAMEL_VERSION="' + meson.project_version() + '"',
	language: 'c',
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)

subdir('protocol')

caramel_inc = include_directories('src')

executable(
	'caramel',
	'src/caramel.c',
	install: true,
)

executable(
	'carameld',
	'src/carameld.c',
	'src/wayland/registry.c',
	include_directories: caramel_inc,
	dependencies: [wayland_client, caramel_protocols_dep],
	install: true,
)
