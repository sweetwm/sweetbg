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
	# Linux-only daemon; enable memfd_create and the POSIX surface
	'-D_GNU_SOURCE',
	language: 'c',
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
libpng = dependency('libpng')
libjpeg = dependency('libjpeg')
wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)

subdir('protocol')

caramel_inc = include_directories('src')

# Framing protocol shared by the client and daemon
ipc_proto = static_library(
	'caramel-ipc',
	'src/ipc/protocol.c',
	include_directories: caramel_inc,
)

executable(
	'caramel',
	'src/caramel.c',
	'src/ipc/client.c',
	include_directories: caramel_inc,
	link_with: ipc_proto,
	install: true,
)

executable(
	'carameld',
	'src/carameld.c',
	'src/wayland/registry.c',
	'src/wayland/output.c',
	'src/wayland/surface.c',
	'src/wayland/shm.c',
	'src/ipc/server.c',
	'src/image/image.c',
	'src/image/fit.c',
	'src/image/png.c',
	'src/image/jpeg.c',
	include_directories: caramel_inc,
	link_with: ipc_proto,
	dependencies: [wayland_client, caramel_protocols_dep, libpng, libjpeg],
	install: true,
)

subdir('tests')
