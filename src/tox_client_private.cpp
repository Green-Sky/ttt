#include "./tox_client_private.hpp"

namespace ttt {

std::unique_ptr<ToxClient> _tox_client;
std::mutex _tox_client_mutex;

void tox_client_save(void) {
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

std::string tox_get_own_address(const Tox *tox) {
	uint8_t self_addr[TOX_ADDRESS_SIZE] = {};
	tox_self_get_address(tox, self_addr);
	std::string own_tox_id_stringyfied;
	own_tox_id_stringyfied.resize(TOX_ADDRESS_SIZE*2 + 1, '\0');
	sodium_bin2hex(own_tox_id_stringyfied.data(), own_tox_id_stringyfied.size(), self_addr, TOX_ADDRESS_SIZE);
	own_tox_id_stringyfied.resize(TOX_ADDRESS_SIZE*2); // remove '\0'

	return own_tox_id_stringyfied;
}

void tox_friend_send_message(const uint32_t friend_number, const TOX_MESSAGE_TYPE type, const std::string& msg) {
	tox_friend_send_message(_tox_client->tox, friend_number, type, reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), nullptr);
}

} // ttt

