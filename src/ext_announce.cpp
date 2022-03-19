#include "./ext_announce.hpp"

#include "./tox_client_private.hpp"

#include <variant>
#include <vector>
#include <algorithm>

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

void ToxExtAnnounce::deregister_ext(ToxExt* toxext) {
	toxext_deregister(_tee);
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

void ToxExtAnnounce::tick(void) {
	for (const auto& [friend_id, compatiple] : friend_compatiple) {
		if (!compatiple) {
			continue;
		}

		if (tox_friend_get_connection_status(_tox_client->tox, friend_id, nullptr) == TOX_CONNECTION_NONE) {
			continue; // TODO: better handle offline friends
		}

		auto& friend_timer = friend_announce_timer[friend_id];
		friend_timer.timer += 0.005f;
		if (friend_timer.timer >= announce_interval) {
			friend_timer.timer = 0.f;

			const std::lock_guard dblock(_tox_client->torrent_db_mutex);
			if (_tox_client->torrent_db.torrents.empty()) {
				continue; // nothing to announce
			}

			// add time
			for (auto& [time, torrent] : friend_timer.torrent_timers) {
				time += announce_interval;
			}

			size_t self_count = 0;

			// update client specific torrent timers
			for (const auto& [_torrent, t_i] : _tox_client->torrent_db.torrents) {
				if (t_i.self) { // no relay, so no gossip
					self_count++;
					const auto& torrent = _torrent; // why the f*** do i need this?????
					auto res_it = std::find_if(
						friend_timer.torrent_timers.cbegin(), friend_timer.torrent_timers.cend(),
						[&](const auto& it) -> bool {
							return it.second == torrent;
						}
					);
					if (res_it == friend_timer.torrent_timers.cend()) {
						const float initial_time = 1000000.f; // HACK: new torrents get time prio
						friend_timer.torrent_timers.emplace_back(std::make_pair(initial_time, torrent));
					}
				}
			}

			if (self_count == 0) {
				continue; // nothing to announce
			}

			// sort by time
			std::sort(
				friend_timer.torrent_timers.begin(), friend_timer.torrent_timers.end(),
				[](const auto& lhs, const auto& rhs) {
					return lhs.first > rhs.first;
				}
			);

			// extract the torrent with highest time (since last announce)
			ext::AnnounceInfoHashPackage aihp{};
			for (size_t i = 0; i < friend_timer.torrent_timers.size() && i < ext::AnnounceInfoHashPackage::info_hashes_max_size; i++) {
			//for (size_t i = 0; i < friend_timer.torrent_timers.size() && i < 1; i++) {
				auto& [time, torrent] = friend_timer.torrent_timers.at(i);
				std::cout << "announce " << friend_id << " " << torrent << " " << time <<  "s\n";
				if (torrent.info_hash_v1) {
					aihp.info_hashes.push_back(*torrent.info_hash_v1);
				} else if (torrent.info_hash_v2) {
					aihp.info_hashes.push_back(*torrent.info_hash_v2);
				} else {
					std::cerr << "!!! invalid torrent without info hash :(\n";
				}
				time = 0.f;
			}

			if (!_tox_client->announce_send(friend_id, aihp)) {
				std::cerr << "!!! failed to announce " << friend_id << "\n";
			}
		}
	}
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
		tdb_ref.torrent_tox_info.friends.emplace(friend_id);
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
	ud->tea->friend_compatiple[friend_id] = compatible;
}

} // ttt::ext

