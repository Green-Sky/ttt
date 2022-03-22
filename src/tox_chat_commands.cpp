#include "./tox_chat_commands.hpp"
#include "ext_tunnel_udp.hpp"
#include "tracker.hpp"

#include <limits>
#include <optional>
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <string_view>

namespace ttt {

//void chat_command_(uint32_t friend_number, std::string_view params);

// ordered :P
const static std::map<std::string, ChatCommand> chat_commands = {
	// general
	{{"help"},					{ToxClient::PermLevel::USER, chat_command_help, "list this help"}},
	{{"info"},					{ToxClient::PermLevel::USER, [](auto, auto){}, "general info, including tracker url, friend count, uptime and transfer rates"}},
	{{"list"},					{ToxClient::PermLevel::USER, chat_command_list, "lists info hashes"}},
	{{"list_magnet"},			{ToxClient::PermLevel::USER, chat_command_list_magnet, "lists info hashes as magnet links"}},
	{{"myaddress"},				{ToxClient::PermLevel::ADMIN, chat_command_myaddress, "get the address to add"}},

	{{"tox_restart"},			{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "restarts the tox thread"}},

	// friends
	{{"friend_list"},			{ToxClient::PermLevel::ADMIN, chat_command_friend_list, "list friends (with perm)"}},
	{{"friend_add"},			{ToxClient::PermLevel::ADMIN, chat_command_friend_add, "<addr> - add friend"}},
	{{"friend_remove"},			{ToxClient::PermLevel::ADMIN, chat_command_friend_remove, "<pubkey> - remove friend"}},
	{{"friend_permission_set"},	{ToxClient::PermLevel::ADMIN, chat_command_friend_permission_set, "<pubkey> <permlvl> - set permission level for fiend"}},
	{{"friend_permission_get"},	{ToxClient::PermLevel::ADMIN, chat_command_friend_permission_get, "<pubkey>"}},
	//{{"friend_allow_transfer"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<pubkey> - allow a friend to use ttt"}},

	// todo: ngc

	// tunnel
	{{"tunnel_host_set"},		{ToxClient::PermLevel::ADMIN, chat_command_tunnel_host_set, "<string> - sets a new tunnel host, default is 127.0.0.1, but torrentclients tend to ignore loopback addr"}},
	{{"tunnel_host_get"},		{ToxClient::PermLevel::ADMIN, chat_command_tunnel_host_get, ""}},

	// TODO: move this comment to help
	// this info is used for remote peers trying to connect. (todo: implement tracker defined port, since it knows)
	{{"torrent_client_host_set"}, {ToxClient::PermLevel::ADMIN, chat_command_torrent_client_host_set, "<string> - sets the host your torrent program is running on, defualt is localhost"}},
	{{"torrent_client_host_get"}, {ToxClient::PermLevel::ADMIN, chat_command_torrent_client_host_get, ""}},
	{{"torrent_client_port_set"}, {ToxClient::PermLevel::ADMIN, chat_command_torrent_client_port_set, "<string> - sets the port your torrent program is running on, defualt is 51413"}},
	{{"torrent_client_port_get"}, {ToxClient::PermLevel::ADMIN, chat_command_torrent_client_port_get, ""}},

	// tracker
	{{"tracker_restart"},		{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "restarts the tracker thread"}},
	{{"tracker_http_host_set"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the trackers listen host, default is localhost. only change this if you want it to be reachalbe from elsewhere."}},
	{{"tracker_http_host_get"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},
	{{"tracker_http_port_set"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, "<string> - sets the trackers listen port, default is 8000."}},
	{{"tracker_http_port_get"},	{ToxClient::PermLevel::ADMIN, [](auto, auto){}, ""}},
};

// TODO: move string utils
static std::string_view trim_prefix(std::string_view sv) {
	while (!sv.empty() && std::isspace(sv.front())) {
		sv.remove_prefix(1);
	}

	return sv;
}

static std::string_view trim_suffix(std::string_view sv) {
	while (!sv.empty() && std::isspace(sv.back())) {
		sv.remove_suffix(1);
	}

	return sv;
}

static std::string_view trim(std::string_view sv) {
	return trim_suffix(trim_prefix(sv));
}

void friend_handle_chat_command(uint32_t friend_number, std::string_view message) {
	// size check was before, so atleast 2 chars
	if (message[0] != '!') {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, "not a command, type !help to learn more.");
		return;
	}
	message = message.substr(1); // cut first char

	std::string_view mc = message;
	auto f_pos = message.find(' ');
	if (f_pos != message.npos) {
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

	cmd.fn(friend_number, trim(message.substr(mc.size())));
	// TODO: access log
}

// assumes trimmed string
static std::vector<std::string_view> cc_split_params(std::string_view params) {
	std::vector<std::string_view> ret;

	auto pos = params.find_first_of(" \t\n\r");
	while (pos != std::string_view::npos) {
		ret.push_back(params.substr(0, pos));

		params = trim_prefix(params.substr(pos));

		pos = params.find_first_of(" \t\n\r");
	}

	if (!params.empty()) {
		ret.push_back(params);
	}

	return ret;
}

static std::vector<std::string_view> cc_prepare_params(uint32_t friend_number, std::string_view params, size_t expected_count) {
	auto params_vec = cc_split_params(params);
	//assert(!params_vec.empty());
	if (params_vec.size() < expected_count) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "too few parameters");
		return {};
	}
	if (params_vec.size() > expected_count) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "too many parameters");
		return {};
	}
	return params_vec;
}

void chat_command_help(uint32_t friend_number, std::string_view) {
	std::string reply {};
	reply += "commands:\n";
	for (const auto& [cmd_str, cmd] : chat_commands) {
		if (_tox_client->friend_has_perm(friend_number, cmd.perm_level)) {
			reply += "  !" + cmd_str + " " + cmd.desc + "\n";
		}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

void chat_command_list(uint32_t friend_number, std::string_view) {
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

			reply += " self:";
			reply += entry.second.self ? "true" : "false";

			reply += " friends:";
			for (const uint32_t f : entry.second.torrent_tox_info.friends) {
				reply += std::to_string(f) + ",";
			}

			reply += "\n";
		}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

void chat_command_list_magnet(uint32_t friend_number, std::string_view) {
	std::string reply {"currently indexed:\n"};
	{
		const std::lock_guard mutex_lock(_tox_client->torrent_db_mutex);
			for (const auto& entry : _tox_client->torrent_db.torrents) {
				reply += "  - ";

				//v1: magnet:?xt=urn:btih:<info-hash>&dn=<name>&tr=<tracker-url>&x.pe=<peer-address>
				if (entry.first.info_hash_v1) {
					reply += "  magnet:?xt=urn:btih:" + std::to_string(*entry.first.info_hash_v1)
						//+ "&dn=name" // TODO: more meta info
						+ "&tr=http://localhost:8000/announce" // TODO: fetch tacker url
						//+ "&x.pe=localhost:5555" // TODO: even peers
					;

				}

				// TODO: v2 (not magnet v2, magnets for torrent v2)
#if 0
				if (entry.first.info_hash_v2) {
					reply += "v2:" + std::to_string(*entry.first.info_hash_v2) + ";";
				}
#endif

				reply += "\n";
			}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

void chat_command_myaddress(uint32_t friend_number, std::string_view) {
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, tox_get_own_address(_tox_client->tox));
}

void chat_command_friend_list(uint32_t friend_number, std::string_view) {
	std::vector<uint32_t> friend_list;
	friend_list.resize(tox_self_get_friend_list_size(_tox_client->tox));
	tox_self_get_friend_list(_tox_client->tox, friend_list.data());

	std::string reply {"friend list: (count: " + std::to_string(friend_list.size()) + ")\n"};

	// TODO: propper error checking
	for (uint32_t friend_list_number : friend_list) {
		TOX_ERR_FRIEND_QUERY err_f_query;

		////f.connection_status = tox_friend_get_connection_status(_tox, friend_number, &err_f_query);
		////assert(err_f_query == TOX_ERR_FRIEND_QUERY_OK);

		std::string f_name{};
		f_name.resize(tox_friend_get_name_size(_tox_client->tox, friend_list_number, &err_f_query));
		//assert(err_f_query == TOX_ERR_FRIEND_QUERY_OK);
		tox_friend_get_name(_tox_client->tox, friend_list_number, reinterpret_cast<uint8_t*>(f_name.data()), &err_f_query);
		//assert(err_f_query == TOX_ERR_FRIEND_QUERY_OK);
		if (err_f_query != TOX_ERR_FRIEND_QUERY_OK) {
			f_name = "-UNK-";
		}

		std::vector<uint8_t> f_pubkey_binary{};
		f_pubkey_binary.resize(TOX_PUBLIC_KEY_SIZE);
		tox_friend_get_public_key(_tox_client->tox, friend_list_number, f_pubkey_binary.data(), nullptr);
		std::string f_pubkey = bin2hex(f_pubkey_binary);

		reply += "  " + f_name + ": " + f_pubkey + "\n";
	}

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

void chat_command_friend_add(uint32_t friend_number, std::string_view params) {
	if (params.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <address>");
		return;
	}

	if (params.length() != TOX_ADDRESS_SIZE*2) {
		std::string reply {"<address> parameter of wrong length " + std::to_string(params.length()) + " should be " + std::to_string(TOX_ADDRESS_SIZE*2)};
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, reply);
		return;
	}

	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		return;
	}

	if (!tox_add_friend(std::string{params_vec.front()})) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "error adding friend");
	} else {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "added friend");
	}
}

void chat_command_friend_remove(uint32_t friend_number, std::string_view params) {
	if (params.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <pubkey>");
		return;
	}

	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		return;
	}

	auto pubkey = hex2bin(std::string{params_vec.front()});
	if (pubkey.size() != TOX_PUBLIC_KEY_SIZE) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "pubkey has the wrong length");
		return;
	}

	Tox_Err_Friend_By_Public_Key e_fbpk = TOX_ERR_FRIEND_BY_PUBLIC_KEY_NULL;
	uint32_t other_friend_number = tox_friend_by_public_key(_tox_client->tox, pubkey.data(), &e_fbpk);
	if (e_fbpk != TOX_ERR_FRIEND_BY_PUBLIC_KEY_OK) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend not found");
		return;
	}

	if (!tox_friend_delete(_tox_client->tox, other_friend_number, nullptr)) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend remove error?");
		return;
	}

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend removed");
	return;
}

static std::optional<ToxClient::PermLevel> str2perm(std::string_view sv) {
	if (sv == "NONE") {
		return ToxClient::PermLevel::NONE;
	} else if (sv == "USER") {
		return ToxClient::PermLevel::USER;
	} else if (sv == "ADMIN") {
		return ToxClient::PermLevel::ADMIN;
	}

	return std::nullopt;
}

static const char* perm2str(ToxClient::PermLevel perm) {
	switch (perm) {
		case ToxClient::PermLevel::NONE: return "NONE";
		case ToxClient::PermLevel::USER: return "USER";
		case ToxClient::PermLevel::ADMIN: return "ADMIN";
		default: return "UNK";
	}
}

void chat_command_friend_permission_set(uint32_t friend_number, std::string_view params) {
	if (params.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <pubkey> <permlvl>");
		return;
	}

	auto params_vec = cc_prepare_params(friend_number, params, 2);
	if (params_vec.empty()) {
		return;
	}

	auto pubkey = hex2bin(std::string{params_vec.front()});
	if (pubkey.size() != TOX_PUBLIC_KEY_SIZE) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "pubkey has the wrong length");
		return;
	}

	Tox_Err_Friend_By_Public_Key e_fbpk = TOX_ERR_FRIEND_BY_PUBLIC_KEY_NULL;
	uint32_t other_friend_number = tox_friend_by_public_key(_tox_client->tox, pubkey.data(), &e_fbpk);
	if (e_fbpk != TOX_ERR_FRIEND_BY_PUBLIC_KEY_OK) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend not found");
		return;
	}

	auto perm = str2perm(params_vec.back());
	if (!perm) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "invalid permission");
		return;
	}

	_tox_client->friend_perms[other_friend_number] = *perm;

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "permission updated");
}

void chat_command_friend_permission_get(uint32_t friend_number, std::string_view params) {
	if (params.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <pubkey>");
		return;
	}

	if (params.length() != TOX_PUBLIC_KEY_SIZE*2) {
		std::string reply {"<pubkey> parameter of wrong length " + std::to_string(params.length()) + " should be " + std::to_string(TOX_PUBLIC_KEY_SIZE*2)};
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, reply);
		return;
	}

	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		return;
	}

	auto pubkey = hex2bin(std::string{params_vec.front()});
	if (pubkey.size() != TOX_PUBLIC_KEY_SIZE) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "pubkey has the wrong length");
		return;
	}

	Tox_Err_Friend_By_Public_Key e_fbpk = TOX_ERR_FRIEND_BY_PUBLIC_KEY_NULL;
	uint32_t other_friend_number = tox_friend_by_public_key(_tox_client->tox, pubkey.data(), &e_fbpk);
	if (e_fbpk != TOX_ERR_FRIEND_BY_PUBLIC_KEY_OK) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend not found");
		return;
	}

	if (!_tox_client->friend_perms.count(other_friend_number)) {
		_tox_client->friend_perms[other_friend_number] = _tox_client->friend_default_perm;
	}

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "friend PermLevel: " + std::string{perm2str(_tox_client->friend_perms[other_friend_number])});
}

void chat_command_tunnel_host_set(uint32_t friend_number, std::string_view params) {
	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <host>");
		return;
	}

	std::string new_host {params_vec.front()};
	tracker_set_tunnel_host(new_host);

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "set host to " + new_host);
}

void chat_command_tunnel_host_get(uint32_t friend_number, std::string_view params) {
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "NOT IMPLEMENTED");
}

void chat_command_torrent_client_host_set(uint32_t friend_number, std::string_view params) {
	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <host> (default is 127.0.0.1)");
		return;
	}

	auto* ext_tunnel = static_cast<ext::ToxExtTunnelUDP*>(_tox_client->extensions.at(1).get());

	std::string new_host {params_vec.front()};
	zed_net_address_t new_addr {};
	if (zed_net_get_address(&new_addr, new_host.c_str(), ext_tunnel->outbound_address.port) != 0) {
		// TODO: better error message
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "error setting host");
		return;
	}

	ext_tunnel->outbound_address = new_addr;

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "set new host " + new_host);
}

void chat_command_torrent_client_host_get(uint32_t friend_number, std::string_view) {
	auto* ext_tunnel = static_cast<ext::ToxExtTunnelUDP*>(_tox_client->extensions.at(1).get());
	const char* host_str = zed_net_host_to_str(ext_tunnel->outbound_address.host);
	if (host_str == nullptr) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "error getting host");
		return;
	}

	std::string reply {"torrent_client host: "};
	reply += host_str;
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, reply);
}

void chat_command_torrent_client_port_set(uint32_t friend_number, std::string_view params) {
	auto params_vec = cc_prepare_params(friend_number, params, 1);
	if (params_vec.empty()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "missing parameters <port>");
		return;
	}

	std::string new_port {params_vec.front()};
	uint64_t new_port_num_tmp {0};
	try {
		new_port_num_tmp = std::stoul(new_port);
	} catch(...) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "invalid port");
		return;
	}

	if (new_port_num_tmp > std::numeric_limits<uint16_t>::max()) {
		tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "invalid port, too large");
		return;
	}

	auto* ext_tunnel = static_cast<ext::ToxExtTunnelUDP*>(_tox_client->extensions.at(1).get());
	ext_tunnel->outbound_address.port = new_port_num_tmp;

	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, "set new port " + std::to_string(new_port_num_tmp));
}

void chat_command_torrent_client_port_get(uint32_t friend_number, std::string_view) {
	auto* ext_tunnel = static_cast<ext::ToxExtTunnelUDP*>(_tox_client->extensions.at(1).get());
	std::string reply {"torrent_client port: "};
	reply += std::to_string(ext_tunnel->outbound_address.port);
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE_NORMAL, reply);
}

} // ttt

