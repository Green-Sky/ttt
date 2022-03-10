cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(tox_torrent_tunnel C CXX)

########################################

add_library(torrent_base_lib STATIC
	./torrent.hpp
	./torrent.cpp
	./torrent_db.hpp
)

target_compile_features(torrent_base_lib PUBLIC cxx_std_17)

########################################

add_library(torrent_tracker_lib STATIC
	./tracker.hpp
	./tracker.cpp
)

target_compile_features(torrent_tracker_lib PUBLIC cxx_std_17)

target_link_libraries(torrent_tracker_lib
	torrent_base_lib
	mongoose
)

########################################

add_executable(tox_torrent_tunnel
	./tox_client_private.hpp
	./tox_client_private.cpp
	./tox_client.hpp
	./tox_client.cpp
	./tox_chat_commands.hpp
	./tox_chat_commands.cpp
	./standalone.cpp
)

target_link_libraries(tox_torrent_tunnel
	torrent_base_lib
	torrent_tracker_lib
	toxcore
)

