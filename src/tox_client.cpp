#include "./tox_client.hpp"

#include <tox/tox.h>

#include <sodium.h>

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

static std::unique_ptr<ToxClient> _tox_client;
static std::mutex _tox_client_mutex;

static void tox_client_save(void);
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

void tox_add_friend(const std::string& addr) {
	const std::lock_guard lock(_tox_client_mutex);

	if (!_tox_client) {
		std::cerr << "tox_client not running!\n";
		return;
	}

	std::array<uint8_t, TOX_ADDRESS_SIZE> addr_bin{};

	Tox_Err_Friend_Add e_fa {TOX_ERR_FRIEND_ADD_NULL};
	tox_friend_add(_tox_client->tox, addr_bin.data(), reinterpret_cast<const uint8_t*>(""), 1, &e_fa);
}

// api end

static std::string tox_get_own_address(const Tox *tox) {
	uint8_t self_addr[TOX_ADDRESS_SIZE] = {};
	tox_self_get_address(tox, self_addr);
	std::string own_tox_id_stringyfied;
	own_tox_id_stringyfied.resize(TOX_ADDRESS_SIZE*2 + 1, '\0');
	sodium_bin2hex(own_tox_id_stringyfied.data(), own_tox_id_stringyfied.size(), self_addr, TOX_ADDRESS_SIZE);
	own_tox_id_stringyfied.resize(TOX_ADDRESS_SIZE*2); // remove '\0'

	return own_tox_id_stringyfied;
}

static void tox_client_save(void) {
	std::vector<uint8_t> savedata{};
	savedata.resize(tox_get_savedata_size(_tox_client->tox));
	tox_get_savedata(_tox_client->tox, savedata.data());
	std::ofstream ofile{_tox_client->savedata_filename, std::ios::binary};
	for (const auto& ch : savedata) {
		ofile.put(ch);
	}
	//ofile.flush();
	ofile.close(); // TODO: do i need this
}

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

	const char *status_message = "toxorrenting";
	tox_self_set_status_message(_tox_client->tox, reinterpret_cast<const uint8_t*>(status_message), std::strlen(status_message), NULL);

	return true;
}

static void tox_client_thread_fn(void) {
	const float save_interval = 60.f * 15.f;
	float save_timer = save_interval/2.f; // initial save
	while (true) {
		{ // lock sope
			const std::lock_guard lock(_tox_client_mutex);
			tox_iterate(_tox_client->tox, nullptr);

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

static void tox_friend_send_message(const uint32_t friend_number, const TOX_MESSAGE_TYPE type, const std::string& msg) {
	tox_friend_send_message(_tox_client->tox, friend_number, type, reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), nullptr);
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
	std::cout << "friend_connection_status_cb\n";
	_tox_client->state_dirty_save_soon = true;
}
//static void friend_typing_cb(Tox *tox, uint32_t friend_number, bool is_typing, void *user_data);
//static void friend_read_receipt_cb(Tox *tox, uint32_t friend_number, uint32_t message_id, void *user_data);
static void friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *) {
	std::cout << "friend_request_cb\n";
	// DIRTY HACK
	tox_friend_add_norequest(tox, public_key, nullptr);
	_tox_client->state_dirty_save_soon = true;
}

static void chat_command_help(uint32_t friend_number, std::string_view params);
static void chat_command_list(uint32_t friend_number, std::string_view params);

struct ChatCommand {
	ToxClient::PermLevel perm_level {ToxClient::PermLevel::ADMIN}; // default to admin, just in case

	std::function<void(uint32_t friend_number, std::string_view params)> fn;

	std::string desc {}; // for help
};
// ordered :P
const static std::map<std::string, ChatCommand> chat_commands {
	// general
	{{"help"},					{ToxClient::PermLevel::USER, chat_command_help, "list this help"}},
	{{"info"},					{ToxClient::PermLevel::USER, [](auto, auto){}, "general info, including tracker url, friend count, uptime and transfer rates"}},
	{{"list"},					{ToxClient::PermLevel::USER, chat_command_list, "lists info hashes"}},
	{{"list_magnet"},			{ToxClient::PermLevel::USER, [](auto, auto){}, "lists info hashes as magnet links"}},
	{{"myaddress"},				{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "get the address to add"}},

	{{"tox_restart"},			{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "restarts the tox thread"}},

	// friends
	{{"friend_list"},			{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "list friends (with perm)"}},
	{{"friend_add"},			{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<addr> - add friend"}},
	{{"friend_remove"},			{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<pubkey> - remove friend"}},
	{{"friend_permission_set"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<pubkey> <permlvl> - set permission level for fiend"}},
	{{"friend_permission_get"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<pubkey>"}},
	//{{"friend_allow_transfer"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<pubkey> - allow a friend to use ttt"}},

	// todo: ngc

	// tunnel
	{{"tunnel_set_host"},		{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets a new tunnel host, default is localhost, but torrentclients tend to ignore loopback addr"}},
	{{"tunnel_get_host"},		{ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},

	// TODO: move this comment to help
	// this info is used for remote peers trying to connect. (todo: implement tracker defined port, since it knows)
	{{"torrent_client_host_set"}, {ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the host your torrent program is running on, defualt is localhost"}},
	{{"torrent_client_host_get"}, {ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},
	{{"torrent_client_port_set"}, {ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the port your torrent program is running on, defualt is 51413"}},
	{{"torrent_client_port_get"}, {ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},

	// tracker
	{{"tracker_restart"},		{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "restarts the tracker thread"}},
	{{"tracker_http_host_set"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the trackers listen host, default is localhost. only change this if you want it to be reachalbe from elsewhere."}},
	{{"tracker_http_host_get"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},
	{{"tracker_http_port_set"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the trackers listen port, default is 8000."}},
	{{"tracker_http_port_get"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},
};
static void chat_command_help(uint32_t friend_number, std::string_view) {
	std::string reply {};
	reply += "commands:\n";
	for (const auto& [cmd_str, cmd] : chat_commands) {
		if (_tox_client->friend_has_perm(friend_number, cmd.perm_level)) {
			reply += "  !" + cmd_str + " " + cmd.desc + "\n";
		}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

static void chat_command_list(uint32_t friend_number, std::string_view) {
	std::string reply {"currently indexed:\n"};
	{
		const std::lock_guard mutex_lock(_tox_client->torrent_db_mutex);
			for (const auto& entry : _tox_client->torrent_db.torrents) {
				reply += "  - ";

				if (entry.first.info_hash_v1) {
					reply += "v1:" + std::to_string(*entry.first.info_hash_v1) + ";";
				}

				if (entry.first.info_hash_v2) {
					reply += "v2:" + std::to_string(*entry.first.info_hash_v2) + ";";
				}

				reply += "\n";
			}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
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


	// size check was before, so atleast 2 chars
	std::string_view m_view(reinterpret_cast<const char*>(message), length);
	if (m_view[0] != '!') {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, "not a command, type !help to learn more.");
		return;
	}
	m_view = m_view.substr(1); // cut first char

	std::string_view mc = m_view;
	auto f_pos = m_view.find(' ');
	if (f_pos != m_view.npos) {
		mc.remove_suffix(mc.size()-f_pos);
	}

	std::string mc_str {mc}; // waiting for c++20
	if (!chat_commands.count(mc_str)) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, "invalid command, type !help to learn more.");
		return;
	}

	const auto& cmd = chat_commands.at(mc_str);
	if (!_tox_client->friend_has_perm(friend_number, cmd.perm_level)) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, "missing permissions");
		return;
	}

	// TODO: fix param
	if (!cmd.fn) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, "missing command implementation, screen at green!");
		return;
	}
	cmd.fn(friend_number, m_view);

#if 0
	if (m_view == "!help") {
	} else if (m_view == "!list") {
		std::string reply {"currently indexed:\n"};
		{
			const std::lock_guard mutex_lock(_tox_client->torrent_db_mutex);
				for (const auto& entry : _tox_client->torrent_db.torrents) {
					reply += "  - ";

					if (entry.first.info_hash_v1) {
						reply += "v1:" + std::to_string(*entry.first.info_hash_v1) + ";";
					}

					if (entry.first.info_hash_v2) {
						reply += "v2:" + std::to_string(*entry.first.info_hash_v2) + ";";
					}

					reply += "\n";
				}
		}
		tox_friend_send_message(tox, friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
	}
#endif
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
static void friend_lossy_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void *user_data) {
	std::cout << "friend_lossy_packet_cb\n";
}

static void friend_lossless_packet_cb(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void *user_data) {
	std::cout << "friend_lossless_packet_cb\n";
}

} // ttt

