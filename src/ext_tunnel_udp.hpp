#pragma once

#include "./ext.hpp"

#include <mongoose.h>

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

		bool udp_send(uint32_t friend_number, uint8_t* data, size_t data_size);

	public: // tox_client "interface"
		// ext support
		// if an entry exists, negotiantion is done
		std::map<uint32_t, bool> friend_compatiple {};

	public: // internal for callbacks
		struct UserData {
			ttt::ToxClient* tc;
			ToxExtTunnelUDP* tetu;
		} ud{};

	private: // internal for mg
		mg_mgr _mgr{};
};

} // ttt::ext

