#include "./tox_chat_commands.hpp"

#include <string>
#include <map>
#include <functional>
#include <mutex>

namespace ttt {

// ordered :P
const static std::map<std::string, ChatCommand> chat_commands = {
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
	cmd.fn(friend_number, message);
	// TODO: access log
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

				reply += "\n";
			}
	}
	tox_friend_send_message(friend_number, TOX_MESSAGE_TYPE::TOX_MESSAGE_TYPE_NORMAL, reply);
}

} // ttt

