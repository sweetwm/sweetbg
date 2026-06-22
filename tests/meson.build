test_ipc_protocol = executable(
	'test-ipc-protocol',
	'test_ipc_protocol.c',
	include_directories: caramel_inc,
	link_with: ipc_proto,
)

test('ipc protocol framing', test_ipc_protocol)

test_image_fit = executable(
	'test-image-fit',
	'test_image_fit.c',
	'../src/image/fit.c',
	include_directories: caramel_inc,
)

test('image cover fit', test_image_fit)
