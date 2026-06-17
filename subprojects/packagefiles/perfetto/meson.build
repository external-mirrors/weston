project('perfetto',
	['cpp'],
	default_options : ['cpp_std=c++20']
)

cpp = meson.get_compiler('cpp')

deps_perfetto = [dependency('threads')]

lib_perfetto = static_library(
	'perfetto',
	sources: 'perfetto.cc',
	dependencies: deps_perfetto,
	install: true,
)

inc_perfetto = include_directories('.')

dep_perfetto = declare_dependency(
	link_with: lib_perfetto,
	include_directories: inc_perfetto,
)
