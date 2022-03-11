#pragma once

#include "./tox_client.hpp"

#include <tox/tox.h>

#include <sodium.h>

#include <memory>
#include <thread>
#include <fstream>
#include <map>
#include <cstring>
#include <cassert>

#include <iostream>
#include <vector>

namespace ttt {

struct ToxClient {
	TorrentDB& torrent_db;
	std::mutex& torrent_db_mutex;

	ToxClient(TorrentDB& torrent_db_, std::mutex& torrent_db_mutex_) : torrent_db(torrent_db_), torrent_db_mutex(torrent_db_mutex_) {
	}

	~ToxClient(void) {
		if (tox) {
			tox_kill(tox);
			tox = nullptr;
		}
	}


	Tox* tox = nullptr;

	std::string savedata_filename {"ttt.tox"};
	bool state_dirty_save_soon {false}; // set in callbacks

	enum PermLevel {
		NONE,
		USER,
		ADMIN,
	};
	std::map<uint32_t, PermLevel> friend_perms {
		{0, ADMIN} // HACK: make first friend admin
	};
	PermLevel friend_default_perm = USER;

	// perm level equal or greater
	bool friend_has_perm(const uint32_t friend_number, const PermLevel perm) {
		if (!friend_perms.count(friend_number)) {
			friend_perms[friend_number] = friend_default_perm;
		}

		return friend_perms[friend_number] >= perm;
	}

	std::thread thread;
};

// globals, hah
extern std::unique_ptr<ToxClient> _tox_client;
extern std::mutex _tox_client_mutex;

void tox_client_save(void);

std::vector<uint8_t> hex2bin(const std::string& str);
std::string bin2hex(const std::vector<uint8_t>& bin);

bool tox_add_friend(const std::string& addr);

std::string tox_get_own_address(const Tox *tox);

void tox_friend_send_message(const uint32_t friend_number, const TOX_MESSAGE_TYPE type, const std::string& msg);

} // ttt

