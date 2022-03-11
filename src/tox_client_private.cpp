#include "./tox_client_private.hpp"
#include <tox/tox.h>

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

std::vector<uint8_t> hex2bin(const std::string& str) {
	std::vector<uint8_t> bin{};
	bin.resize(str.size()/2, 0);

	sodium_hex2bin(bin.data(), bin.size(), str.c_str(), str.length(), nullptr, nullptr, nullptr);

	return bin;
}

std::string bin2hex(const std::vector<uint8_t>& bin) {
	std::string str{};
	str.resize(TOX_ADDRESS_SIZE*2, '\0');

	sodium_bin2hex(str.data(), str.size(), bin.data(), bin.size());

	return str;
}

bool tox_add_friend(const std::string& addr) {
	//const std::lock_guard lock(_tox_client_mutex); // private

	//if (!_tox_client) {
		//std::cerr << "tox_client not running!\n";
		//return;
	//}

	//std::array<uint8_t, TOX_ADDRESS_SIZE> addr_bin{};

	//sodium_hex2bin(addr_bin.data(), addr_bin.size(), addr.c_str(), addr.length(), nullptr, nullptr, nullptr);
	auto addr_bin = hex2bin(addr);
	if (addr_bin.size() != TOX_ADDRESS_SIZE) {
		return false;
	}

	Tox_Err_Friend_Add e_fa {TOX_ERR_FRIEND_ADD_NULL};
	tox_friend_add(_tox_client->tox, addr_bin.data(), reinterpret_cast<const uint8_t*>("i am ttt"), 1, &e_fa);

	return e_fa == TOX_ERR_FRIEND_ADD_OK;
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

