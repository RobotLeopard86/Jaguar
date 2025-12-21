# Install headers
install_subdir('include' / 'libjaguar', install_dir: 'include')

# libjaguar
libjaguar = both_libraries('jaguar', sources: [
	'src' / 'Reader.cpp'
], include_directories: 'include', pic: true, install: true)

# Dependency
libjaguar_dep = declare_dependency(link_with: libjaguar, include_directories: 'include')