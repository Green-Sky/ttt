#include "./tox_client.hpp"

#include "./tox_client_private.hpp"
#include "./tox_chat_commands.hpp"
#include "toxext.h"

#include <memory>
#include <string>
#include <thread>
#include <fstream>
#include <map>
#include <functional>
#include <cstring>
#include <cassert>

#include <iostream>

namespace ttt {

static bool tox_client_setup(void);

static void tox_client_thread_fn(void);

bool tox_client_start(TorrentDB& torrent_db, std::mutex& torrent_db_mutex) {
	const std::lock_guard lock(_tox_client_mutex);

	if (_tox_client) {
		std::cerr << "tox_client already running!!!!!!!!\n";
		return false;
	}

	_tox_client = std::make_unique<ToxClient>(torrent_db, torrent_db_mutex);

	if (!tox_client_setup()) { // error
		_tox_client.reset(nullptr);
		return false;
	}

	// TODO: block and stall until connected to dht

	_tox_client->thread = std::thread(tox_client_thread_fn);

	return true;
}

void tox_client_stop(void) {
	{
		const std::lock_guard lock(_tox_client_mutex);

		if (!_tox_client) {
			std::cerr << "tox_client not running!\n";
			return;
		}
	}

	// join thread

	const std::lock_guard lock(_tox_client_mutex);

	tox_kill(_tox_client->tox);


	_tox_client.reset(nullptr);
}

// restart

// api end

// ============ tox callbacks ============

// logging
static void log_cb(Tox *tox, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func, const char *message, void *user_data);

// self
static void self_connection_status_cb(Tox *tox, TOX_CONNECTION connection_status, void *user_data);

// friend
static void friend_name_cb(Tox *tox, uint32_t friend_number, const uint8_t *name, size_t length, void *user_data);
//static void friend_status_message_cb(Tox *tox, uint32_t friend_number, const uint8_t *message, size_t length, void *user_data);
//static void friend_status_cb(Tox *tox, uint32_t friend_number, TOX_USER_STATUS status, void *user_data);
static void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status, void *user_data);
//static void friend_typing_cb(Tox *tox, uint32_t friend_number, bool is_typing, void *user_data);
//static void friend_read_receipt_cb(Tox *tox, uint32_t friend_number, uint32_t message_id, void *user_data);
static void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data);
static void friend_message_cb(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data);

// file
//static void file_recv_control_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control, void *user_data);
//static void file_chunk_request_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length, void *user_data);
//static void file_recv_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint32_t kind, uint64_t file_size, const uint8_t *filename, size_t filename_length, void *user_data);
//static void file_recv_chunk_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, const uint8_t *data, size_t length, void *user_data);

// conference
//static void conference_invite_cb(Tox *tox, uint32_t friend_number, TOX_CONFERENCE_TYPE type, const uint8_t *cookie, size_t length, void *user_data);
//static void conference_connected_cb(Tox *tox, uint32_t conference_number, void *user_data);
//static void conference_message_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data);
//static void conference_title_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, const uint8_t *title, size_t length, void *user_data);
//static void conference_peer_name_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, const uint8_t *name, size_t length, void *user_data);
//static void conference_peer_list_changed_cb(Tox *tox, uint32_t conference_number, void *user_data);

// custom packets
static void friend_lossy_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void *user_data);
static void friend_lossless_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void *user_data);


static void tox_client_setup_callbacks(Tox* tox) {
	//tox_callback_self_connection_status(tox, self_connection_status_cb);
#define CALLBACK_REG(x) tox_callback_##x(tox, x##_cb)
	CALLBACK_REG(self_connection_status);

	CALLBACK_REG(friend_name);
	//CALLBACK_REG(friend_status_message);
	//CALLBACK_REG(friend_status);
	CALLBACK_REG(friend_connection_status);
	//CALLBACK_REG(friend_typing);
	//CALLBACK_REG(friend_read_receipt);
	CALLBACK_REG(friend_request);
	CALLBACK_REG(friend_message);

	//CALLBACK_REG(file_recv_control);
	//CALLBACK_REG(file_chunk_request);
	//CALLBACK_REG(file_recv);
	//CALLBACK_REG(file_recv_chunk);

	//CALLBACK_REG(conference_invite);
	//CALLBACK_REG(conference_connected);
	//CALLBACK_REG(conference_message);
	//CALLBACK_REG(conference_title);
	//CALLBACK_REG(conference_peer_name);
	//CALLBACK_REG(conference_peer_list_changed);

	CALLBACK_REG(friend_lossy_packet);
	CALLBACK_REG(friend_lossless_packet);

#undef CALLBACK_REG
}

static bool tox_client_setup(void) {
	{ // scope for save data (optional)
		TOX_ERR_OPTIONS_NEW err_opt_new;
		Tox_Options* options = tox_options_new(&err_opt_new);
		assert(err_opt_new == TOX_ERR_OPTIONS_NEW::TOX_ERR_OPTIONS_NEW_OK);
		tox_options_set_log_callback(options, log_cb);
#ifndef USE_TEST_NETWORK
		tox_options_set_local_discovery_enabled(options, true);
#endif
		tox_options_set_udp_enabled(options, true);
		tox_options_set_hole_punching_enabled(options, true);

		std::ifstream ifile{_tox_client->savedata_filename, std::ios::binary};
		std::vector<uint8_t> savedata{};
		if (ifile.is_open()) {
			std::cout << "TOX loading save " << _tox_client->savedata_filename << "\n";
			// fill savedata
			while (ifile.good()) {
				auto ch = ifile.get();
				if (ch == EOF) {
					break;
				} else {
					savedata.push_back(ch);
				}
			}

			if (savedata.empty()) {
				std::cerr << "empty tox save\n";
			} else {
				// set options
				tox_options_set_savedata_type(options, TOX_SAVEDATA_TYPE_TOX_SAVE);
				tox_options_set_savedata_data(options, savedata.data(), savedata.size());

				// ignore free, savedata gets out of scope eventually...
			}

			ifile.close(); // do i need this?
		}

		TOX_ERR_NEW err_new;
		_tox_client->tox = tox_new(options, &err_new);
		tox_options_free(options);
		if (err_new != TOX_ERR_NEW_OK) {
			std::cerr << "tox_new failed with error code " << err_new << "\n";
			return false;
		}
	}

	// immediatly save
	tox_client_save();

	// print own address
	std::cout << "created tox instance with addr:" << tox_get_own_address(_tox_client->tox) << "\n";

	{ // setup ext
		_tox_client->tox_ext = toxext_init(_tox_client->tox);
		// TODO: test null
		for (auto& ext : _tox_client->extensions) {
			ext->register_ext(_tox_client->tox_ext);
		}
	}

	tox_client_setup_callbacks(_tox_client->tox);

	// dht bootstrap
	{
		struct DHT_node {
			const char *ip;
			uint16_t port;
			const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1]; // 1 for null terminator
			unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
		};

		DHT_node nodes[] =
		{
#ifndef USE_TEST_NETWORK
			// own bootsrap node, to reduce load
			{"g-s.xyz",								42581,	"785E369A3449A374032E1E08D0AE2CF4A23793F6AC998B7383FE32B0C7D0F37F", {}}, // dev
			{"tox.plastiras.org",					33445,	"8E8B63299B3D520FB377FE5100E65E3322F7AE5B20A0ACED2981769FC5B43725", {}}, // 14
			{"tox.plastiras.org",					443,	"8E8B63299B3D520FB377FE5100E65E3322F7AE5B20A0ACED2981769FC5B43725", {}}, // 14
			{"104.244.74.69",						33445,	"8E8B63299B3D520FB377FE5100E65E3322F7AE5B20A0ACED2981769FC5B43725", {}}, // 14
#if 0
			// TODO: update data
			{"tox.verdict.gg",						33445,	"1C5293AEF2114717547B39DA8EA6F1E331E5E358B35F9B6B5F19317911C5F976", {}},
			{"78.46.73.141",						33445,	"02807CF4F8BB8FB390CC3794BDF1E8449E9A8392C5D3F2200019DA9F1E812E46", {}},
			{"2a01:4f8:120:4091::3",				33445,	"02807CF4F8BB8FB390CC3794BDF1E8449E9A8392C5D3F2200019DA9F1E812E46", {}},
			{"tox.abilinski.com",					33445,	"10C00EB250C3233E343E2AEBA07115A5C28920E9C8D29492F6D00B29049EDC7E", {}},
			{"tox.novg.net",						33445,	"D527E5847F8330D628DAB1814F0A422F6DC9D0A300E6C357634EE2DA88C35463", {}},
			{"198.199.98.108",						33445,	"BEF0CFB37AF874BD17B9A8F9FE64C75521DB95A37D33C5BDB00E9CF58659C04F", {}},
			{"2604:a880:1:20::32f:1001",			33445,	"BEF0CFB37AF874BD17B9A8F9FE64C75521DB95A37D33C5BDB00E9CF58659C04F", {}},
			{"tox.kurnevsky.net",					33445,	"82EF82BA33445A1F91A7DB27189ECFC0C013E06E3DA71F588ED692BED625EC23", {}},
			{"87.118.126.207",						33445,	"0D303B1778CA102035DA01334E7B1855A45C3EFBC9A83B9D916FFDEBC6DD3B2E", {}},
			{"81.169.136.229",						33445,	"E0DB78116AC6500398DDBA2AEEF3220BB116384CAB714C5D1FCD61EA2B69D75E", {}},
			{"2a01:238:4254:2a00:7aca:fe8c:68e0:27ec",	33445,	"E0DB78116AC6500398DDBA2AEEF3220BB116384CAB714C5D1FCD61EA2B69D75E", {}},
			{"205.185.115.131",						53,		"3091C6BEB2A993F1C6300C16549FABA67098FF3D62C6D253828B531470B53D68", {}},
			{"205.185.115.131",						33445,	"3091C6BEB2A993F1C6300C16549FABA67098FF3D62C6D253828B531470B53D68", {}},
			{"205.185.115.131",						443,	"3091C6BEB2A993F1C6300C16549FABA67098FF3D62C6D253828B531470B53D68", {}},
			{"tox2.abilinski.com",					33445,	"7A6098B590BDC73F9723FC59F82B3F9085A64D1B213AAF8E610FD351930D052D", {}},
			{"floki.blog",							33445,	"6C6AF2236F478F8305969CCFC7A7B67C6383558FF87716D38D55906E08E72667", {}},
#endif
#else // testnet
			{"tox.plastiras.org",					38445,	"5E47BA1DC3913EB2CBF2D64CE4F23D8BFE5391BFABE5C43C5BAD13F0A414CD77", {}}, // 14
#endif
		};

		for (size_t i = 0; i < sizeof(nodes)/sizeof(DHT_node); i ++) {
			sodium_hex2bin(
				nodes[i].key_bin, sizeof(nodes[i].key_bin),
				nodes[i].key_hex, sizeof(nodes[i].key_hex)-1,
				NULL, NULL, NULL
			);
			tox_bootstrap(_tox_client->tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, NULL);
			// TODO: use extra tcp option to avoid error msgs
			// ... this is hardcore
			tox_add_tcp_relay(_tox_client->tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, NULL);
		}
	}

	const char *name = "ttt";
	tox_self_set_name(_tox_client->tox, reinterpret_cast<const uint8_t*>(name), std::strlen(name), NULL);

	const char *status_message = "toxorrenelling";
	tox_self_set_status_message(_tox_client->tox, reinterpret_cast<const uint8_t*>(status_message), std::strlen(status_message), NULL);

	return true;
}

static void tox_client_thread_fn(void) {
	const float save_interval = 60.f * 15.f;
	float save_timer = save_interval/2.f; // initial save

	// TODO: real time

	while (true) {
		{ // lock sope
			const std::lock_guard lock(_tox_client_mutex);
			tox_iterate(_tox_client->tox, nullptr);
			toxext_iterate(_tox_client->tox_ext); // is this right??

			for (auto& [friend_id, timer] : _tox_client->friend_announce_timer) {
				timer += 0.005f;
				if (timer >= _tox_client->announce_interval) {
					timer = 0.f;
					// announce
				}
			}

			if (save_timer >= save_interval || _tox_client->state_dirty_save_soon) {
				save_timer = 0.f;
				_tox_client->state_dirty_save_soon = false;
				std::cerr << "TOX III SAVE\n";

				tox_client_save();
			}
		}

		using namespace std::literals;
		std::this_thread::sleep_for(5ms);
		save_timer += 0.005f;
	}
}

// logging
static void log_cb(Tox *tox, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func, const char *message, void *) {
	std::cerr << "TOX " << level << " " << file << ":" << line << "(" << func << ") " << message << "\n";
}

// self
static void self_connection_status_cb(Tox *tox, TOX_CONNECTION connection_status, void *) {
	std::cout << "self_connection_status_cb\n";
	_tox_client->state_dirty_save_soon = true;
}

// friend
static void friend_name_cb(Tox *tox, uint32_t friend_number, const uint8_t *name, size_t length, void *) {
	std::cout << "friend_name_cb\n";
}
//static void friend_status_message_cb(Tox *tox, uint32_t friend_number, const uint8_t *message, size_t length, void *user_data);
//static void friend_status_cb(Tox *tox, uint32_t friend_number, TOX_USER_STATUS status, void *user_data);
static void friend_connection_status_cb(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status, void *) {
	std::cout << "friend_connection_status_cb " << friend_number << "\n";
	_tox_client->state_dirty_save_soon = true;

	// negotiate toxext extensions
	if (connection_status != TOX_CONNECTION::TOX_CONNECTION_NONE) {
		for (auto& ext : _tox_client->extensions) {
			ext->negotiate_connection(friend_number);
		}
	}
}
//static void friend_typing_cb(Tox *tox, uint32_t friend_number, bool is_typing, void *user_data);
//static void friend_read_receipt_cb(Tox *tox, uint32_t friend_number, uint32_t message_id, void *user_data);
static void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *) {
	std::cout << "friend_request_cb\n";

	Tox_Err_Friend_Add e_fa = TOX_ERR_FRIEND_ADD::TOX_ERR_FRIEND_ADD_OK;
	uint32_t new_fren = tox_friend_add_norequest(tox, public_key, &e_fa);
	if (e_fa == TOX_ERR_FRIEND_ADD::TOX_ERR_FRIEND_ADD_OK) {
		_tox_client->friend_announce_timer[new_fren] = 0.f;
	}
	_tox_client->state_dirty_save_soon = true;
}

static void friend_message_cb(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *) {
	std::cout << "friend_message_cb\n";

	if (!_tox_client->friend_has_perm(friend_number, ToxClient::PermLevel::USER)) {
		std::string reply {"you dont have permission to use this bot"};
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
		return;
	}

	if (length < 2) {
		return;
	}

	friend_handle_chat_command(friend_number, std::string_view(reinterpret_cast<const char*>(message), length));
}

// file
//static void file_recv_control_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control, void *user_data);
//static void file_chunk_request_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length, void *user_data);
//static void file_recv_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint32_t kind, uint64_t file_size, const uint8_t *filename, size_t filename_length, void *user_data);
//static void file_recv_chunk_cb(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, const uint8_t *data, size_t length, void *user_data);

// conference
//static void conference_invite_cb(Tox *tox, uint32_t friend_number, TOX_CONFERENCE_TYPE type, const uint8_t *cookie, size_t length, void *user_data);
//static void conference_connected_cb(Tox *tox, uint32_t conference_number, void *user_data);
//static void conference_message_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data);
//static void conference_title_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, const uint8_t *title, size_t length, void *user_data);
//static void conference_peer_name_cb(Tox *tox, uint32_t conference_number, uint32_t peer_number, const uint8_t *name, size_t length, void *user_data);
//static void conference_peer_list_changed_cb(Tox *tox, uint32_t conference_number, void *user_data);

// custom packets
static void friend_lossy_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void*) {
	std::cout << "friend_lossy_packet_cb\n";
}

static void friend_lossless_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void*) {
	std::cout << "friend_lossless_packet_cb\n";
	const auto toxext_ret = toxext_handle_lossless_custom_packet(_tox_client->tox_ext, friend_number, data, length);
	std::cout << "toxext_handle " << toxext_ret << "\n";
}

} // ttt

