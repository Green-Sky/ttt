#pragma once

#include "./torrent_db.hpp"

#include <string>
#include <mutex>

namespace ttt {

	// all functions are thread save

	void tox_client_start(TorrentDB& torrent_db, std::mutex& torrent_db_mutex);
	void tox_client_stop(void);

	// restart

	void tox_add_friend(const std::string& addr);

} // ttt

