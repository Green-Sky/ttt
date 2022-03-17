#include "./ext_announce.hpp"

#include "./tox_client_private.hpp"

namespace ttt::ext {

// fist 12 bytes are the same for all ttt
// last byte denotes version for the extention
constexpr static uint8_t announce_uuid[16] {
	0x11, 0x13, 0xf4, 0xf7,
	0x93, 0x19, 0x66, 0x5a,
	0x22, 0xc2, 0xb5, 0xee,

	0x11, 0x13, 0x31, 0x00,
};

struct UserData {
	ttt::ToxClient* tc;
	ToxExtAnnounce* tea;
};

static void announce_recv_callback(
	ToxExtExtension* extension,
	uint32_t friend_id,
	void const* data, size_t size,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

static void announce_negotiate_connection_callback(
	ToxExtExtension* extension,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

void ToxExtAnnounce::register_ext(ToxExt* toxext) {
	ud.tc = _tox_client.get();
	ud.tea = this;

	tee = toxext_register(
		toxext, announce_uuid, &ud, 
		announce_recv_callback,
		announce_negotiate_connection_callback
	);
	assert(tee);

	std::cout << "III register_ext announce\n";
}

bool ToxExtAnnounce::announce_send(uint32_t friend_number, const AnnounceInfoHashPackage& aihp) {
	return true;
}

static void announce_recv_callback(
	ToxExtExtension*,
	uint32_t friend_id, void const* data,
	size_t size, void* userdata,
	struct ToxExtPacketList* response_packet_list
) {
	std::cout << "III announce_recv_callback\n";
	auto* ud = static_cast<ToxExtAnnounce::UserData*>(userdata);
}

static void announce_negotiate_connection_callback(
	ToxExtExtension*,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
) {
	std::cout << "III announce_negotiate_connection_callback " << friend_id << " " << compatible << "\n";
	auto* ud = static_cast<ToxExtAnnounce::UserData*>(userdata);
	ud->tc->friend_compatiple[friend_id] = compatible;
}

} // ttt::ext

