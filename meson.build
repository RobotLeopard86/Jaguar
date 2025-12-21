project('jaguar', 'cpp', version: '0.1.0', meson_version: '>=1.2.0', license: 'Apache-2.0', default_options: ['cpp_std=c++20', 'default_library=static', 'b_vscrt=from_buildtype'])

bt = get_option('buildtype')

# Define debug macro
if bt.contains('debug')
	add_project_arguments(['-D_DEBUG'], language: 'cpp')
endif

# Set some Windows-specific options
if host_machine.system() == 'windows'
	add_project_arguments(['-D_DLL', '-D_MT', '-DLJBUILD'], language: 'cpp')
	if bt == 'debug'
		add_project_arguments('-D_ITERATOR_DEBUG_LEVEL=2', language: 'cpp')
	endif
endif

# Build libjaguar
subdir('libjaguar')

# Build jaguartool if requested
if get_option('jaguartool')
	subdir('tool')
endif
