lib_png_writer = static_library(
	'png-writer',
	sources: [
		'png-writer.c',
	],
	include_directories: common_inc,
	dependencies: [
		dependency('libpng'),
		dep_pixman,
		dep_libweston_public,
	],
	install: false,
)

dep_png_writer = declare_dependency(
	link_with: lib_png_writer,
	dependencies: dep_pixman,
	include_directories: public_inc,
)

executable(
	'png-write-test',
	sources: 'png-write-test.c',
	include_directories: common_inc,
	dependencies: dep_png_writer,
	install: false,
)
