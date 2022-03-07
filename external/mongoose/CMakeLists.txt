cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

add_library(mongoose
	./mongoose/mongoose.h
	./mongoose/mongoose.c
)

target_include_directories(mongoose
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mongoose/"
)

