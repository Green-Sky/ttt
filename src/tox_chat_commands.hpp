#pragma once

#include "./tox_client_private.hpp"

#include <string>
#include <string_view>
#include <map>
#include <functional>

namespace ttt {

void friend_handle_chat_command(uint32_t friend_number, std::string_view message);

struct ChatCommand {
	ToxClient::PermLevel perm_level {ToxClient::PermLevel::ADMIN}; // default to admin, just in case

	std::function<void(uint32_t friend_number, std::string_view params)> fn;

	std::string desc {}; // for help
};

//const std::map<std::string, ChatCommand> chat_commands;

void chat_command_help(uint32_t friend_number, std::string_view params);
void chat_command_list(uint32_t friend_number, std::string_view params);

} // ttt

