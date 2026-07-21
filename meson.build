project(
	'sweetbg',
	'c',
	version: '0.2.0',
	meson_version: '>= 0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
	],
)

add_project_arguments(
	'-DSWEETBG_VERSION="' + meson.project_version() + '"',
	# Linux-only daemon; enable memfd_create and the POSIX surface
	'-D_GNU_SOURCE',
	language: 'c',
)

fs = import('fs')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
libpng = dependency('libpng')
libjpeg = dependency('libjpeg')
libwebp = dependency('libwebp')

# jpeg.c decodes straight into JCS_EXT_BGRX, a libjpeg-turbo extension
cc = meson.get_compiler('c')
if not cc.has_header_symbol(
	'jpeglib.h',
	'JCS_EXT_BGRX',
	prefix: '#include <stdio.h>',
	dependencies: libjpeg,
)
	error('libjpeg-turbo is required: jpeglib.h does not define JCS_EXT_BGRX')
endif
wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)

subdir('protocol')

sweetbg_inc = include_directories('src')

# Framing protocol shared by the client and daemon
ipc_proto = static_library(
	'sweetbg-ipc',
	'src/ipc/protocol.c',
	include_directories: sweetbg_inc,
)

executable(
	'sweetbg',
	'src/sweetbg.c',
	'src/doctor/doctor.c',
	'src/ipc/client.c',
	'src/image/image.c',
	'src/image/fit.c',
	'src/image/layout.c',
	'src/image/pick.c',
	'src/image/palette.c',
	'src/image/png.c',
	'src/image/jpeg.c',
	'src/image/webp.c',
	'src/config/config.c',
	'src/config/config_write.c',
	include_directories: sweetbg_inc,
	link_with: ipc_proto,
	dependencies: [libpng, libjpeg, libwebp],
	install: true,
)

executable(
	'sweetbgd',
	'src/sweetbgd.c',
	'src/wayland/registry.c',
	'src/wayland/output.c',
	'src/wayland/surface.c',
	'src/wayland/shm.c',
	'src/ipc/server.c',
	'src/config/config.c',
	'src/query/json.c',
	include_directories: sweetbg_inc,
	link_with: ipc_proto,
	dependencies: [wayland_client, sweetbg_protocols_dep],
	install: true,
)

subdir('doc')
subdir('systemd')

if fs.exists('tests/meson.build')
	subdir('tests')
endif
