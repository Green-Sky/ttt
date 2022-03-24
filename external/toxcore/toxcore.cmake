set(TOX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/c-toxcore/")

# TODO: shared
add_library(toxcore STATIC
	${TOX_DIR}toxcore/ccompat.c
	${TOX_DIR}toxcore/ccompat.h
	${TOX_DIR}toxcore/crypto_core.c
	${TOX_DIR}toxcore/crypto_core.h

	${TOX_DIR}toxcore/logger.c
	${TOX_DIR}toxcore/logger.h
	${TOX_DIR}toxcore/mono_time.c
	${TOX_DIR}toxcore/mono_time.h
	${TOX_DIR}toxcore/network.c
	${TOX_DIR}toxcore/network.h
	${TOX_DIR}toxcore/state.c
	${TOX_DIR}toxcore/state.h
	${TOX_DIR}toxcore/util.c
	${TOX_DIR}toxcore/util.h

	${TOX_DIR}toxcore/DHT.c
	${TOX_DIR}toxcore/DHT.h
	${TOX_DIR}toxcore/LAN_discovery.c
	${TOX_DIR}toxcore/LAN_discovery.h
	${TOX_DIR}toxcore/ping.c
	${TOX_DIR}toxcore/ping.h
	${TOX_DIR}toxcore/ping_array.c
	${TOX_DIR}toxcore/ping_array.h

	${TOX_DIR}toxcore/TCP_client.c
	${TOX_DIR}toxcore/TCP_client.h
	${TOX_DIR}toxcore/TCP_common.h
	${TOX_DIR}toxcore/TCP_common.c
	${TOX_DIR}toxcore/TCP_connection.c
	${TOX_DIR}toxcore/TCP_connection.h
	${TOX_DIR}toxcore/TCP_server.c
	${TOX_DIR}toxcore/TCP_server.h
	${TOX_DIR}toxcore/list.c
	${TOX_DIR}toxcore/list.h
	${TOX_DIR}toxcore/net_crypto.c
	${TOX_DIR}toxcore/net_crypto.h
	${TOX_DIR}toxcore/onion.c
	${TOX_DIR}toxcore/onion.h
	${TOX_DIR}toxcore/onion_announce.c
	${TOX_DIR}toxcore/onion_announce.h
	${TOX_DIR}toxcore/onion_client.c
	${TOX_DIR}toxcore/onion_client.h

	${TOX_DIR}toxcore/friend_connection.c
	${TOX_DIR}toxcore/friend_connection.h
	${TOX_DIR}toxcore/friend_requests.c
	${TOX_DIR}toxcore/friend_requests.h

	${TOX_DIR}toxcore/Messenger.c
	${TOX_DIR}toxcore/Messenger.h

	${TOX_DIR}toxcore/group.c
	${TOX_DIR}toxcore/group.h

	${TOX_DIR}toxcore/tox_api.c
	${TOX_DIR}toxcore/tox.c
	${TOX_DIR}toxcore/tox.h
	${TOX_DIR}toxcore/tox_private.h

	${TOX_DIR}toxcore/events/conference_connected.c
	${TOX_DIR}toxcore/events/conference_invite.c
	${TOX_DIR}toxcore/events/conference_message.c
	${TOX_DIR}toxcore/events/conference_peer_list_changed.c
	${TOX_DIR}toxcore/events/conference_peer_name.c
	${TOX_DIR}toxcore/events/conference_title.c
	${TOX_DIR}toxcore/events/file_chunk_request.c
	${TOX_DIR}toxcore/events/file_recv.c
	${TOX_DIR}toxcore/events/file_recv_chunk.c
	${TOX_DIR}toxcore/events/file_recv_control.c
	${TOX_DIR}toxcore/events/friend_connection_status.c
	${TOX_DIR}toxcore/events/friend_lossless_packet.c
	${TOX_DIR}toxcore/events/friend_lossy_packet.c
	${TOX_DIR}toxcore/events/friend_message.c
	${TOX_DIR}toxcore/events/friend_name.c
	${TOX_DIR}toxcore/events/friend_read_receipt.c
	${TOX_DIR}toxcore/events/friend_request.c
	${TOX_DIR}toxcore/events/friend_status.c
	${TOX_DIR}toxcore/events/friend_status_message.c
	${TOX_DIR}toxcore/events/friend_typing.c
	${TOX_DIR}toxcore/events/events_alloc.c
	${TOX_DIR}toxcore/events/events_alloc.h
	${TOX_DIR}toxcore/events/self_connection_status.c
	${TOX_DIR}toxcore/bin_pack.c
	${TOX_DIR}toxcore/bin_pack.h
	${TOX_DIR}toxcore/bin_unpack.c
	${TOX_DIR}toxcore/bin_unpack.h
	${TOX_DIR}toxcore/tox_events.c
	${TOX_DIR}toxcore/tox_events.h
	${TOX_DIR}toxcore/tox_unpack.c
	${TOX_DIR}toxcore/tox_unpack.h
)

#HACK: "install" api headers into self
# this is dirty, should be binary dir
# TODO: add the others
configure_file(
	${TOX_DIR}toxcore/tox.h
	${TOX_DIR}tox/tox.h
	@ONLY
)

target_include_directories(toxcore PRIVATE "${TOX_DIR}toxcore")
target_include_directories(toxcore PUBLIC "${TOX_DIR}")

target_compile_definitions(toxcore PUBLIC USE_IPV6=1)
#target_compile_definitions(toxcore PUBLIC MIN_LOGGER_LEVEL=LOGGER_LEVEL_DEBUG)

target_link_libraries(toxcore msgpackc)

#find_package(unofficial-sodium CONFIG REQUIRED)
#target_link_libraries(toxcore unofficial-sodium::sodium unofficial-sodium::sodium_config_public)
find_package(unofficial-sodium CONFIG QUIET)
find_package(sodium QUIET)
if(unofficial-sodium_FOUND) # vcpkg
	target_link_libraries(toxcore unofficial-sodium::sodium unofficial-sodium::sodium_config_public)
elseif(sodium_FOUND)
	target_link_libraries(toxcore sodium)
else()
	message(SEND_ERROR "missing libsodium")
endif()

# set c99 and c++11
#target_set

#if(NSL_LIBRARIES)
  #set(toxcore_LINK_MODULES ${toxcore_LINK_MODULES} ${NSL_LIBRARIES})
  #set(toxcore_PKGCONFIG_LIBS ${toxcore_PKGCONFIG_LIBS} -lnsl)
#endif()

#if(RT_LIBRARIES)
  #set(toxcore_LINK_MODULES ${toxcore_LINK_MODULES} ${RT_LIBRARIES})
  #set(toxcore_PKGCONFIG_LIBS ${toxcore_PKGCONFIG_LIBS} -lrt)
#endif()

#if(SOCKET_LIBRARIES)
  #set(toxcore_LINK_MODULES ${toxcore_LINK_MODULES} ${SOCKET_LIBRARIES})
  #set(toxcore_PKGCONFIG_LIBS ${toxcore_PKGCONFIG_LIBS} -lsocket)
#endif()

if(WIN32)
	target_link_libraries(toxcore ws2_32 iphlpapi)
endif()

find_package(Threads REQUIRED)
target_link_libraries(toxcore Threads::Threads)

add_executable(DHT_Bootstrap EXCLUDE_FROM_ALL
	${TOX_DIR}other/DHT_bootstrap.c
	${TOX_DIR}other/bootstrap_node_packets.h
	${TOX_DIR}other/bootstrap_node_packets.c
	${TOX_DIR}testing/misc_tools.h
	${TOX_DIR}testing/misc_tools.c
)

target_link_libraries(DHT_Bootstrap toxcore)

