cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

add_library(toxext
	./toxext/src/toxext_util.h
	./toxext/src/toxext.h
	./toxext/src/toxext.c
)

target_include_directories(toxext
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/toxext/src"
)

target_link_libraries(toxext toxcore)

