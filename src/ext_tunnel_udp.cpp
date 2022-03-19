#include "./ext_tunnel_udp.hpp"

#include "./tox_client_private.hpp"

extern "C" {
// mongoose should be able to be included as c++
#include "mongoose.h"
}

namespace ttt::ext {

// fist 12 bytes are the same for all ttt
// last byte denotes version for the extention
constexpr static uint8_t announce_uuid[16] {
	0x11, 0x13, 0xf4, 0xf7,
	0x93, 0x19, 0x66, 0x5a,
	0x22, 0xc2, 0xb5, 0xee,

	0x11, 0x13, 0x32, 0x00,
};

static void tunnel_udp_recv_callback(
	ToxExtExtension* extension,
	uint32_t friend_id,
	void const* data, size_t size,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

static void tunnel_udp_negotiate_connection_callback(
	ToxExtExtension* extension,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

void ToxExtTunnelUDP::register_ext(ToxExt* toxext) {
	ud.tc = _tox_client.get();
	ud.tetu = this;

	_tee = toxext_register(
		toxext, announce_uuid, &ud,
		tunnel_udp_recv_callback,
		tunnel_udp_negotiate_connection_callback
	);
	assert(_tee);
	std::cout << "III register_ext announce\n";

	mg_mgr_init(&_mgr);

#if 0
	// per friend with infohashes

	//auto* conn = mg_listen(&_mgr, "udp://0.0.0.0:0", nullptr, nullptr);
	// conn->peer; set to torrent client

	// connect to torrent client. we can do this bc udp is connection less.
	// same behaviour as listening and setting the peer manually, but its just 1 call.
	std::string torrent_client_host {"127.0.0.1"};
	std::string torrent_client_port {"1337"};
	std::string url {
		"udp://"
		+ torrent_client_host
		+ ":"
		+ torrent_client_port
	};
	auto* conn = mg_connect(&_mgr, url.c_str(), nullptr, nullptr);
#endif
}

void ToxExtTunnelUDP::deregister_ext(ToxExt* toxext) {
	toxext_deregister(_tee);
	mg_mgr_free(&_mgr);
}

bool ToxExtTunnelUDP::udp_send(uint32_t friend_number, uint8_t* data, size_t data_size) {
	// bc first byte needs to be id, but the udp lib does not
	return true;
}

static void tunnel_udp_recv_callback(
	ToxExtExtension*,
	uint32_t friend_id, const void* data,
	size_t size, void* userdata,
	struct ToxExtPacketList* response_packet_list
) {
	std::cout << "III tunnel_udp_recv_callback??\n";
}

static void tunnel_udp_negotiate_connection_callback(
	ToxExtExtension*,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
) {
	std::cout << "III tunnel_udp_negotiate_connection_callback " << friend_id << " " << compatible << "\n";
	auto* ud = static_cast<ToxExtTunnelUDP::UserData*>(userdata);
	ud->tetu->friend_compatiple[friend_id] = compatible;
}

} // ttt::ext

