#pragma once

#include "./torrent.hpp"

#include <unordered_map>
#include <set>
#include <string>

// contains friend/group ids and timestamps
struct TorrentToxInfo {
	std::set<uint32_t> friends{};
};

struct TorrentDB {
	struct TorrentEntry {
		//Torrent torrent;
		bool self {false};
		TorrentToxInfo torrent_tox_info {};
	};
	std::unordered_map<Torrent, TorrentEntry> torrents {};

	// mapps friend -> port
	// ext_tunnel_udp controlled
	std::unordered_map<uint32_t, uint16_t> peers {};
};

