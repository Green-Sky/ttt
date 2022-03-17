#include "./ext.hpp"

#include "./tox_client_private.hpp"

namespace ttt::ext {

void ToxClientExtension::negotiate_connection(uint32_t friend_number) {
	auto r = toxext_negotiate_connection(tee, friend_number);

	if (r != TOXEXT_SUCCESS) {
		std::cerr << "toxext_negotiate_connection returned error\n";
	}
}

} // ttt::ext
