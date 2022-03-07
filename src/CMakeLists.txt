cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(tox_torrent_tunnel C CXX)

add_library(tox_torrent_tunnel_libs STATIC
	./torrent.hpp
	./torrent.cpp
	./torrent_db.hpp
)

target_compile_features(tox_torrent_tunnel_libs PUBLIC cxx_std_17)

add_executable(tox_torrent_tunnel
	./standalone.cpp
)

target_link_libraries(tox_torrent_tunnel
	tox_torrent_tunnel_libs
)

