#include "./torrent_db.hpp"

#include <mutex>
#include <thread>
#include <iostream>
#include <string>

static TorrentDB torrent_db {};
static std::mutex torrent_db_mutex;

std::ostream& operator<<(std::ostream& os, const Torrent& t) {
	if (t.info_hash_v1) {
		os << "v1:" << std::to_string(*t.info_hash_v1) << ";";
	}

	if (t.info_hash_v2) {
		os << "v2:" << std::to_string(*t.info_hash_v2) << ";";
	}

	return os;
}

int main(int argc, char** argv) {

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

	{ // http tracker thread
	}

	{ // main thread (cli)
		while (true) {
			using namespace std::literals;
			std::this_thread::sleep_for(10ms);
		}
	}

	return 0;
}
