test_ipc_protocol = executable(
	'test-ipc-protocol',
	'test_ipc_protocol.c',
	include_directories: caramel_inc,
	link_with: ipc_proto,
)

test('ipc protocol framing', test_ipc_protocol)
