#pragma once

#include "./torrent_db.hpp"

#include <mutex>
#include <string>
#include <cstdint>

namespace ttt {

	// all functions are thread save

	// start the tracker, torrentdb is the communication between tracker and tunnel
	void tracker_start(TorrentDB& torrent_db, std::mutex& torrent_db_mutex);
	void tracker_stop(void); // blocks until it quits

	// signal tracker restart, blocks until new thread is started
	void tracker_restart(void);

	// default is localhost
	void tracker_set_http_host(const std::string& host);
	// default is 8000
	void tracker_set_http_port(const uint16_t port_in_host_order);

	//{"127.0.0.1"}; // torrent clients discard loopback addresses
	//{"192.168.1.179"}; // so as a workaround you can use your lan address, in this case most torrent programms dont discard it and it should not leave your pc
	void tracker_set_tunnel_host(const std::string& host);

} // ttt

