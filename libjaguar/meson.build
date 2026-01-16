# Install headers
install_subdir('include' / 'libjaguar', install_dir: 'include')

# libjaguar
libjaguar = both_libraries('jaguar', sources: [
	'src' / 'Decoder.cpp',
	'src' / 'Encoder.cpp',
	'src' / 'Reader.cpp',
	'src' / 'Writer.cpp'
], include_directories: ['include', 'src'], pic: true, install: true)

# Dependency
libjaguar_dep = declare_dependency(link_with: libjaguar, include_directories: 'include')