#pragma once

#include "./ext.hpp"

#include <zed_net.h>

#include <map>

namespace ttt {
	struct ToxClient;
} // ttt

namespace ttt::ext {

// this extention does not actually use the toxext protocol, only for negotiation
// after comp check, it just uses lossy directly
class ToxExtTunnelUDP : public ToxClientExtension {
	public:
		ToxExtTunnelUDP(void) = default;

		void register_ext(ToxExt* toxext) override;
		void deregister_ext(ToxExt* toxext) override;

		// creates and destroys tunnels
		void tick(void) override;

		void friend_custom_pkg_cb(uint32_t friend_number, uint8_t* data, size_t size);

	public: // tox_client "interface"
		// ext support
		// if an entry exists, negotiantion has been done at least once
		std::map<uint32_t, bool> friend_compatible {};

	// TODO: friend or static?
	public: // internal for callbacks
		struct UserData {
			ttt::ToxClient* tc;
			ToxExtTunnelUDP* tetu;
		} ud{};

	private: // tunnel api
		//bool udp_send(uint32_t friend_number, uint8_t* data, size_t data_size);

	private: // tunnel data
		struct Tunnel {
			zed_net_socket_t s;
			uint16_t port {}; // in host
		};

		std::map<uint32_t, Tunnel> _tunnels {};
};

} // ttt::ext

