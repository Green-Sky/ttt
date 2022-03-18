#include "./ext_announce.hpp"

#include "./tox_client_private.hpp"
#include <variant>
#include <vector>

namespace ttt::ext {


bool AnnounceInfoHashPackage::to(std::vector<uint8_t>& buff) const {
	if (info_hashes.empty()) {
		return false; // well yes
	}

	for (const auto& ih : info_hashes) {
		if (ih.index() == 0) {
			buff.push_back(0);
			for (const auto& c : std::get<InfoHashV1>(ih).data) {
				buff.push_back(c);
			}
		} else if (ih.index() == 1) {
			buff.push_back(1);
			for (const auto& c : std::get<InfoHashV2>(ih).data) {
				buff.push_back(c);
			}
		} else {
			assert(false && "what");
		}
	}

	return true;
}

bool AnnounceInfoHashPackage::from(const uint8_t* buff, const size_t buff_size) {
	if (buff == nullptr || buff_size == 0) {
		return false;
	}

	const uint8_t* curr_buff = buff;
	const uint8_t* buff_end = buff + buff_size;

#define _CHECK() \
		if (curr_buff >= buff_end) { \
			std::cerr << "!!! error parsing aihp\n"; \
			return false; \
		}

	while (curr_buff < buff_end && info_hashes.size() < info_hashes_max_size) {
		_CHECK();
		uint8_t type_index = *curr_buff++;

		if (type_index == 0) {
			InfoHashV1 info_hash;
			for (uint8_t& c : info_hash.data) {
				_CHECK();
				c = *curr_buff++;
			}
			info_hashes.push_back(info_hash);
		} else if (type_index == 1) {
			InfoHashV2 info_hash;
			for (uint8_t& c : info_hash.data) {
				_CHECK();
				c = *curr_buff++;
			}
			info_hashes.push_back(info_hash);
		} else {
			std::cerr << "!!! error parsing aihp type\n";
			return false;
		}
	}

#undef _CHECK

	return curr_buff == buff_end;
}

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

	_tee = toxext_register(
		toxext, announce_uuid, &ud,
		announce_recv_callback,
		announce_negotiate_connection_callback
	);
	assert(_tee);

	std::cout << "III register_ext announce\n";
}

bool ToxExtAnnounce::announce_send(ToxExt* tox_ext, uint32_t friend_number, const AnnounceInfoHashPackage& aihp) {
	std::vector<uint8_t> buff{};
	if (!aihp.to(buff)) {
		std::cerr << "!!! error creating buffer from aihp\n";
		return false;
	}

	auto* pkg_list = toxext_packet_list_create(tox_ext, friend_number);
	assert(pkg_list);

	toxext_segment_append(pkg_list, _tee, buff.data(), buff.size());

	auto r = toxext_send(pkg_list);
	return r == TOXEXT_SUCCESS;
}

static void announce_recv_callback(
	ToxExtExtension*,
	uint32_t friend_id, const void* data,
	size_t size, void* userdata,
	struct ToxExtPacketList* response_packet_list
) {
	std::cout << "III announce_recv_callback\n";
	auto* ud = static_cast<ToxExtAnnounce::UserData*>(userdata);

	AnnounceInfoHashPackage aihp{};
	if (!aihp.from(static_cast<const uint8_t*>(data), size)) {
		std::cerr << "!!! error, parsed guarbage----\n";
		return;
	}

	const std::lock_guard lock(ud->tc->torrent_db_mutex);
	auto& torrent_db = ud->tc->torrent_db;
	for (const auto& info_hash_var : aihp.info_hashes) {
		Torrent t;
		if (info_hash_var.index() == 0) {
			t.info_hash_v1 = std::get<0>(info_hash_var);
		} else if (info_hash_var.index() == 1) {
			t.info_hash_v2 = std::get<1>(info_hash_var);
		}

		std::cout << "got " << t << " from " << friend_id << "\n";

		auto& tdb_ref = torrent_db.torrents[t]; // wtf why does self get set to true????
		// TODO: save friend_number
		//tdb_ref.torrent_tox_info.
	}
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

