add_library(
	glutil
	STATIC
	include/glutil/gl.h
	include/glutil/object.h
	include/glutil/object_array.h
	src/object.cpp
)

target_include_directories(glutil PUBLIC include)
target_link_libraries(glutil PUBLIC glbinding)
