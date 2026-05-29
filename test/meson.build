tests = [
    'integer_read',
    'float_read',
    'buffer_read',
    'string_read',
    'bool_read',
    'header_fuzz',
    'reader_move_semantics'
]

foreach name : tests
    test(name, executable('TEST_' + name, name + '.cpp', dependencies: libjaguar_dep))
endforeach