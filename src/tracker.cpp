#include "./tracker.hpp"

#include <mongoose.h>

#include <mutex>
#include <memory>
#include <thread>
#include <queue>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

static std::ostream& operator<<(std::ostream& os, const Torrent& t) {
	if (t.info_hash_v1) {
		os << "v1:" << std::to_string(*t.info_hash_v1) << ";";
	}

	if (t.info_hash_v2) {
		os << "v2:" << std::to_string(*t.info_hash_v2) << ";";
	}

	return os;
}

// src : https://marcoarena.wordpress.com/2017/01/03/string_view-odi-et-amo/
static std::vector<std::string_view> split(std::string_view str, const char* delims) {
	std::vector<std::string_view> ret;

	std::string_view::size_type start = 0;
	auto pos = str.find_first_of(delims, start);
	while (pos != std::string_view::npos) {
		if (pos != start) {
			ret.push_back(str.substr(start, pos - start));
		}
		start = pos + 1;
		pos = str.find_first_of(delims, start);
	}
	if (start < str.length())
		ret.push_back(str.substr(start, str.length() - start));

	return ret;
}

static std::string to_bencode(const std::string& value) {
	return std::to_string(value.size()) + ":" + value;
}

static std::string to_bencode(const int64_t& value) {
	return "i" + std::to_string(value) + "e";
}

static std::vector<uint8_t> url_decode(const std::string_view v) {
	std::vector<uint8_t> buf;
	buf.resize(v.size()+1);
	auto new_len = mg_url_decode(v.data(), v.size(), (char*)buf.data(), buf.size(), 0);
	buf.resize(new_len);
	return buf;
}


namespace ttt {

struct Tracker {
	TorrentDB& torrent_db;
	std::mutex& torrent_db_mutex;

	std::string http_host {"localhost"};
	uint16_t http_port {8000};

	bool stop {false};

	std::thread thread;

	Tracker(TorrentDB& torrent_db_, std::mutex& torrent_db_mutex_) : torrent_db(torrent_db_), torrent_db_mutex(torrent_db_mutex_) {
	}

	// is this useles
	enum class TrackerCommand {
		STOP
	};
	std::queue<TrackerCommand> command_queue;
};

static std::unique_ptr<Tracker> _tracker;
static std::mutex _tracker_mutex;

static void http_tracker_thread_fn(void);

// all functions are thread save

// start the tracker, torrentdb is the communication between tracker and tunnel
void tracker_start(TorrentDB& torrent_db, std::mutex& torrent_db_mutex) {
	const std::lock_guard lock(_tracker_mutex);

	if (_tracker) {
		std::cerr << "tracker already running!!!!!!!!\n";
		return;
	}

	// setup tracker
	_tracker = std::make_unique<Tracker>(torrent_db, torrent_db_mutex);

	// start thread
	_tracker->thread = std::thread(http_tracker_thread_fn);
}

void tracker_stop(void) {
	{
		const std::lock_guard lock(_tracker_mutex);

		if (!_tracker) {
			std::cerr << "tracker not running!\n";
			return;
		}

		_tracker->command_queue.push(Tracker::TrackerCommand::STOP);
	}

	// join thread
	_tracker->thread.join();

	const std::lock_guard lock(_tracker_mutex);
	_tracker.reset(nullptr);
}

// signal tracker restart
void tracker_restart(void) {
	{
		const std::lock_guard lock(_tracker_mutex);

		if (!_tracker) {
			std::cerr << "tracker not running!\n";
			return;
		}

		_tracker->command_queue.push(Tracker::TrackerCommand::STOP);
	}
	_tracker->thread.join();

	const std::lock_guard lock(_tracker_mutex);
	// start thread
	_tracker->thread = std::thread(http_tracker_thread_fn);
}

void tracker_set_http_host(const std::string& host) {
	std::cerr << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " NOT IMPLEMENTED!\n";
}

void tracker_set_http_port(const uint16_t port_in_host_order) {
	std::cerr << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " NOT IMPLEMENTED!\n";
}

//{"127.0.0.1"}; // torrent clients discard loopback addresses
//{"192.168.1.179"}; // so as a workaround you can use your lan address, in this case most torrent programms dont discard it and it should not leave your pc
void tracker_set_tunnel_host(const std::string& host) {
	std::cerr << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << " NOT IMPLEMENTED!\n";
}

// api end

// https://wiki.theory.org/index.php/BitTorrentSpecification#Tracker_HTTP.2FHTTPS_Protocol
static void http_handle_announce(mg_connection* c, mg_http_message* hm) {
	if (hm->query.ptr == nullptr) {
		mg_http_reply(c, 101, "Content-Type: text/plain\r\n", "missing info_hash");
	} else {
		//std::string query_str(hm->query.ptr, 0, hm->query.len);
		//mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "query: %s\n", query_str.c_str());

		auto query_split = split({hm->query.ptr, hm->query.len}, "&");
		std::unordered_map<std::string, std::string> query_map;
		for (const auto& e_v : query_split) {
			auto kv = split(e_v, "=");
			if (kv.size() != 2) {
				std::cerr << "kv not 2\n";
				continue;
			}

			// special case
			// can contain \0
			if (kv.at(0) == "info_hash") {
				// easier for internal handling
				query_map[std::string((kv.at(0)))] = std::to_string(url_decode(kv.at(1)));
			} else {
				//query_map[url_decode(kv.at(0))] = url_decode(kv.at(1));
				query_map[std::string((kv.at(0)))] = kv.at(1);
			}
		}

		if (false) { // debug
			std::string list_str {"query:\n"};
			for (const auto& [k,v] : query_map) {
				list_str += "  " + k + ":" + v + "\n";
			}
			mg_http_reply(c, 200, "Content-Type: text/plain\r\n", list_str.c_str());
			return;
		}

		if (!query_map.count("info_hash")) {
			// 101
			mg_http_reply(c, 101, "Content-Type: text/plain\r\n", "missing info_hash");
		}

		// create torrent
		Torrent t;
		std::string info_hash = query_map["info_hash"];
		// sadnes
		if (info_hash.size() == 20*2) { // v1
			t.info_hash_v1 = InfoHashV1{info_hash};
		} else if (info_hash.size() == 32*2) { // v2
			t.info_hash_v2 = InfoHashV2{info_hash};
		} else {
			mg_http_reply(c, 500, "Content-Type: text/plain\r\n", "bruh what, info_hash bonkers");
		}


		{
			// meh
			const std::lock_guard tracker_lock(_tracker_mutex);

			// TODO: replace with messaging ?
			const std::lock_guard mutex_lock(_tracker->torrent_db_mutex);
			if (!_tracker->torrent_db.torrents.count(t)) {
				auto& new_entry = _tracker->torrent_db.torrents[t];
				new_entry.self = true;
				std::cout << "III new info_hash" << t << "\n";
			}

			// TODO: timestamp

			struct Peer {
				//std::string ip {"127.0.0.1"};
				std::string ip {"192.168.1.172"};
				//std::string ip {"127.255.1.1"};
				//uint16_t port {20111};
				uint16_t port {42048};
			};

			std::vector<Peer> peer_list{};
			peer_list.emplace_back(); // default

			std::vector<std::string> response_peer_list{};
			for (const auto& peer : peer_list) {
				response_peer_list.emplace_back(
					"d" +
						to_bencode("ip") + to_bencode(peer.ip) +
						to_bencode("port") + to_bencode(peer.port) +
					"e"
				);
			}

			// response dict key is plain, value is bencoded
			std::unordered_map<std::string, std::string> response_dict{
				{"interval", to_bencode(60)}, // 60s
				{"peers", "l" + response_peer_list[0] + "e"}, // TODO: more peers
			};

			// eg:
			// 	d
			// 		8:interval
			// 			i1800e
			// 		5:peers
			// 			l
			// 				d
			// 					2:ip
			// 						13:192.168.189.1
			// 					4:port
			// 						i20111e
			// 				e
			// 			e
			// 	e
			std::string bencode_response {};
			bencode_response += "d";
			for (const auto& [key, value] : response_dict) {
				bencode_response += to_bencode(key) + value;
			}

			bencode_response += "e";
			mg_http_reply(c, 200, "Content-Type: text/plain\r\n", bencode_response.c_str());
		}

	}
}

static void http_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	if (ev == MG_EV_HTTP_MSG) {
		mg_http_message* hm = (mg_http_message *) ev_data;
		//std::cerr << "got request:" << std::string(hm->message.ptr, 0, hm->message.len) << "\n";
		if (mg_http_match_uri(hm, "/announce")) {
			http_handle_announce(c, hm);

			mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "huh?");
		} else if (mg_http_match_uri(hm, "/list")) {
			std::string list_str {"currently indexed:\n"};

			{
				const std::lock_guard tracker_lock(_tracker_mutex);
				const std::lock_guard mutex_lock(_tracker->torrent_db_mutex);

				for (const auto& entry : _tracker->torrent_db.torrents) {
					list_str += "  - ";

					if (entry.first.info_hash_v1) {
						list_str += "v1:" + std::to_string(*entry.first.info_hash_v1) + ";";
					}

					if (entry.first.info_hash_v2) {
						list_str += "v2:" + std::to_string(*entry.first.info_hash_v2) + ";";
					}

					list_str += "\n";
				}
			}

			mg_http_reply(c, 200, "Content-Type: text/plain\r\n", list_str.c_str());
		} else {
			mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "TTT\n");
		}
	}
}

static void http_tracker_thread_fn(void) {
	mg_mgr mgr;

	{ // setup mongoose
		mg_mgr_init(&mgr);

		const std::lock_guard lock(_tracker_mutex);
		//std::string listen_url {"http://localhost:8000"};
		std::string listen_url {
			"http://" + _tracker->http_host + ":" + std::to_string(_tracker->http_port)
		};

		mg_http_listen(&mgr, listen_url.c_str(), http_fn, &mgr);
	}

	while (true) {
		{ // do commands
			const std::lock_guard lock(_tracker_mutex);
			auto& queue = _tracker->command_queue;
			bool big_break = false;
			while (queue.size()) {
				if (queue.front() == Tracker::TrackerCommand::STOP) {
					_tracker->stop = true; // useles?
					big_break = true;
					break;
				}
			}

			if (big_break) {
				break;
			}
		}

		mg_mgr_poll(&mgr, 1000);

		using namespace std::literals;
		std::this_thread::sleep_for(20ms);
	}

	mg_mgr_free(&mgr);
}

} // ttt


