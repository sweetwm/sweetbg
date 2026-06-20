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

executable(
	'caramel',
	'src/caramel.c',
	install: true,
)

executable(
	'carameld',
	'src/carameld.c',
	install: true,
)
