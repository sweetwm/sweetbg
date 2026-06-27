test_ipc_protocol = executable(
	'test-ipc-protocol',
	'test_ipc_protocol.c',
	include_directories: manju_inc,
	link_with: ipc_proto,
)

test('ipc protocol framing', test_ipc_protocol)

test_image_fit = executable(
	'test-image-fit',
	'test_image_fit.c',
	'../src/image/fit.c',
	include_directories: manju_inc,
)

test('image cover fit', test_image_fit)

test_config = executable(
	'test-config',
	'test_config.c',
	'../src/config/config.c',
	include_directories: manju_inc,
)

test('config parser', test_config)

test_config_write = executable(
	'test-config-write',
	'test_config_write.c',
	'../src/config/config_write.c',
	'../src/config/config.c',
	include_directories: manju_inc,
)

test('config writer', test_config_write)
