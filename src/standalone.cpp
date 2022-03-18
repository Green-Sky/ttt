#include "./torrent_db.hpp"

#include "./tracker.hpp"
#include "tox_client.hpp"

#include <mutex>
#include <thread>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

static TorrentDB torrent_db {};
static std::mutex torrent_db_mutex;

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

#if 0
	{ // dummy data
		const std::lock_guard mutex_lock(torrent_db_mutex);

		{ // ubuntu iso as example
			// magnet:?xt=urn:btih:f09c8d0884590088f4004e010a928f8b6178c2fd&dn=ubuntu-20.04.4-desktop-amd64.iso&tr=https%3A%2F%2Ftorrent.ubuntu.com%2Fannounce&tr=https%3A%2F%2Fipv6.torrent.ubuntu.com%2Fannounce
			Torrent t;
			t.info_hash_v1 = InfoHashV1{"f09c8d0884590088f4004e010a928f8b6178c2fd"};

			TorrentToxInfo tti {
				"nope"
			};

			//torrent_db.torrents[t].torrent_tox_info = tti;
			torrent_db.torrents[t];
		}

		{ // print db
			std::cout << "db:\n";
			for (const auto& entry : torrent_db.torrents) {
				std::cout << "  " << entry.first << "\n";
			}
		}
	}
#endif

	if (!ttt::tox_client_start(torrent_db, torrent_db_mutex)) {
		return -1;
	}

	ttt::tracker_start(torrent_db, torrent_db_mutex);

#if 0
	{ // hack pause main thread for 30s to wait for dht
		using namespace std::literals;
		std::this_thread::sleep_for(30s);
		ttt::tox_add_friend("4159D7CF1C51430FA28EFEFFD2CCF0172632AA55C64B4CD145564D4F4853342F74C87EC2CFF0");
	}
#endif

	{ // main thread (cli)
		while (true) {
			using namespace std::literals;
			std::this_thread::sleep_for(10ms);
		}
	}

	ttt::tracker_stop();

	return 0;
}
