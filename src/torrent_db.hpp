#pragma once

#include "./torrent.hpp"

#include <unordered_map>
#include <string>

// contains friend/group ids and timestamps
struct TorrentToxInfo {
	std::string tmp {"hi"};
};

struct TorrentDB {
	struct TorrentEntry {
		//Torrent torrent;
		bool self {false};
		TorrentToxInfo torrent_tox_info {};
	};
	std::unordered_map<Torrent, TorrentEntry> torrents;
};

