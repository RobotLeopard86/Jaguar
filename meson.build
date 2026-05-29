project('jaguar', 'cpp', version: '1.0.0-pre8', meson_version: '>=1.2.0', license: 'Apache-2.0', default_options: ['cpp_std=c++20', 'default_library=static', 'b_vscrt=from_buildtype'])

bt = get_option('buildtype')

# Define debug macro
if bt.contains('debug')
	add_project_arguments(['-D_DEBUG'], language: 'cpp')
endif

# Set some Windows-specific options
if host_machine.system() == 'windows'
	add_project_arguments(['-D_DLL', '-D_MT', '-DLJBUILD'], language: 'cpp')
	if bt == 'debug'
		add_project_arguments(['-D_ITERATOR_DEBUG_LEVEL=2', '-fms-runtime-lib=dll_dbg'], language: 'cpp')
	else
		add_project_arguments('-fms-runtime-lib=dll', language: 'cpp')
	endif
endif

# Install headers
install_subdir('include' / 'libjaguar', install_dir: 'include')

# libjaguar
libjaguar = both_libraries('jaguar', sources: [
	'src' / 'Decoder.cpp',
	'src' / 'Document.cpp',
	'src' / 'Encoder.cpp',
	'src' / 'Reader.cpp',
	'src' / 'Utilities.cpp',
	'src' / 'Writer.cpp'
], include_directories: ['include', 'src'], pic: true, install: true)

# Dependency
libjaguar_dep = declare_dependency(link_with: libjaguar, include_directories: 'include')

# Tests
subdir('test')
