tests = [
    'basic_types',
    'structured_object',
    'duplicate_name',
    'invalid_utf8',
    'list',
    'nested_object',
    'errors',
	'scoped_view',
	'buffer_transfer',
	'utilities',
	'extended_value_creation',
	'structured_obj_fieldcreate'
]

foreach name : tests
    test(name, executable('TEST_' + name, name + '.cpp', dependencies: libjaguar_dep))
endforeach
