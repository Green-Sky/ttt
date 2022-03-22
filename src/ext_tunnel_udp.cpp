#include "./ext_tunnel_udp.hpp"

#include "./tox_client_private.hpp"

#include <vector>

namespace ttt::ext {

// fist 12 bytes are the same for all ttt
// last byte denotes version for the extention
constexpr static uint8_t announce_uuid[16] {
	0x11, 0x13, 0xf4, 0xf7,
	0x93, 0x19, 0x66, 0x5a,
	0x22, 0xc2, 0xb5, 0xee,

	0x11, 0x13, 0x32, 0x00,
};

static void tunnel_udp_recv_callback(
	ToxExtExtension* extension,
	uint32_t friend_id,
	void const* data, size_t size,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

static void tunnel_udp_negotiate_connection_callback(
	ToxExtExtension* extension,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
);

void ToxExtTunnelUDP::register_ext(ToxExt* toxext) {
	ud.tc = _tox_client.get();
	ud.tetu = this;

	_tee = toxext_register(
		toxext, announce_uuid, &ud,
		tunnel_udp_recv_callback,
		tunnel_udp_negotiate_connection_callback
	);
	assert(_tee);
	std::cout << "III register_ext announce\n";

	// default
	// TODO: load from config
	zed_net_get_address(&outbound_address, "localhost", 51413);
}

void ToxExtTunnelUDP::deregister_ext(ToxExt* toxext) {
	toxext_deregister(_tee);
}

void ToxExtTunnelUDP::tick(void) {
	{ // iterate every tunnel
		for (auto& [f_id, tun] : _tunnels) {
			zed_net_address_t addr{};

			TOX_MAX_CUSTOM_PACKET_SIZE;
			const size_t buff_size_max = TOX_MAX_CUSTOM_PACKET_SIZE-2;
			uint8_t buff[buff_size_max];
			buff[0] = packet_id; // TODO: tox_lossy_pkg_id

			int ret = zed_net_udp_socket_receive(&(tun.s), &addr, buff+1, buff_size_max-1);

			if (ret < 0) {
				std::cerr << "!!! error receiving on socket\n";
				continue;
			} else if (ret == 0) {
				std::cerr << "WWW got empty udp packet\n";
				continue; // no data
			}

			std::cout << "III got udp " << tun.port << "  " << ret << "\n";
			if (ret == buff_size_max-1) {
				std::cerr << "WWW got max sized udp packet\n";
			}

			// TODO: check addr maches torrent client setting, otherwise ignore

			// debug !!!
#if 0
			std::cout << ">>> got udp " << std::hex;
			for (size_t i = 0; i < (size_t)ret; i++) {
				std::cout << (int)buff[i+1] << " ";
			}
			std::cout << std::dec << "\n";
#endif

			// TODO: error checking
			if (!tox_friend_send_lossy_packet(ud.tc->tox, f_id, buff, ret+1, nullptr)) {
				std::cerr << "!!! error sending lossy " << f_id << "  << " "" << ret+1 << "\n";
			}
		}
	}

	// TODO: dont do every tick !!!
	{ // destroy tunnels to offline friends
		std::vector<uint32_t> to_destroy {};
		for (const auto& [f_id, tun] : _tunnels) {
			if (tox_friend_get_connection_status(ud.tc->tox, f_id, nullptr) == TOX_CONNECTION_NONE) {
				to_destroy.push_back(f_id);
			}
		}

		for (const auto& f_id : to_destroy) {
			{ // first stop advertising
				const std::lock_guard mutex_lock{ud.tc->torrent_db_mutex};
				ud.tc->torrent_db.peers.erase(f_id);
			}
			zed_net_socket_close(&_tunnels[f_id].s);
			std::cout << "III closed tunnel " << f_id << " " << _tunnels[f_id].port << "\n";
			_tunnels.erase(f_id);
			friend_compatible.erase(f_id); // also erase from compatible list

		}
	}

	// TODO: dont do every tick !!!
	{ // create new tunnels for friends (TODO: with matching infohashes)
		std::vector<uint32_t> to_destroy {};
		for (const auto& [f_id, comp] : friend_compatible) {
			if (!comp) { // not compatible
				continue;
			}

			if (_tunnels.count(f_id)) { // tunnel exists
				continue;
			}

			if (tox_friend_get_connection_status(ud.tc->tox, f_id, nullptr) == TOX_CONNECTION_NONE) {
				to_destroy.push_back(f_id);
				continue;
			}

			auto& new_tunnel = _tunnels[f_id];
			new_tunnel.port = 0;
			// THIS IS HARDCORE
			// we try every port in range
			for (uint16_t port = 20000; port < 60000; port++) {
				if (zed_net_udp_socket_open(&new_tunnel.s, port, 1) == 0) {
					new_tunnel.port = port;
					std::cout << "III opened socket " << f_id << " " << port << "\n";
					break;
				}
			}

			if (new_tunnel.port == 0) {
				_tunnels.erase(f_id);
				std::cerr << "!!! failed to open socket " << f_id << "\n";
				continue;
			}

			// notify torrent_db of peer
			const std::lock_guard mutex_lock{ud.tc->torrent_db_mutex};
			ud.tc->torrent_db.peers[f_id] = new_tunnel.port;
		}

		// clean up
		for (const auto& f_id : to_destroy) {
			friend_compatible.erase(f_id);
		}
	}
}

void ToxExtTunnelUDP::friend_custom_pkg_cb(uint32_t friend_number, const uint8_t* data, size_t size) {
	std::cout << "<<< friend_custom_pkg_cb " << friend_number << " " << size << "\n";
	if (size < 2) {
		std::cerr << "!!! packet too small\n";
		return;
	}

	if (data[0] != packet_id) {
		std::cerr << "!!! packet id mismatch\n";
		return;
	}

	if (!friend_compatible.count(friend_number) || !friend_compatible.at(friend_number)) {
		std::cerr << "WWW packet from incompatible friend " << friend_number << "\n";
		return;
	}

	if (!_tunnels.count(friend_number)) {
		std::cerr << "!!! packet, but no tunnel yet " << friend_number << "\n";
		return;
	}

	auto& tunnel = _tunnels.at(friend_number);
	// TODO: error check
	zed_net_udp_socket_send(&(tunnel.s), outbound_address, data+1, size-1);
}

static void tunnel_udp_recv_callback(
	ToxExtExtension*,
	uint32_t friend_id, const void* data,
	size_t size, void* userdata,
	struct ToxExtPacketList* response_packet_list
) {
	std::cout << "III tunnel_udp_recv_callback??\n";
}

static void tunnel_udp_negotiate_connection_callback(
	ToxExtExtension*,
	uint32_t friend_id, bool compatible,
	void* userdata,
	ToxExtPacketList* response_packet_list
) {
	std::cout << "III tunnel_udp_negotiate_connection_callback " << friend_id << " " << compatible << "\n";
	auto* ud = static_cast<ToxExtTunnelUDP::UserData*>(userdata);
	ud->tetu->friend_compatible[friend_id] = compatible;
}

} // ttt::ext

