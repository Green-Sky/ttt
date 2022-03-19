#pragma once

#include "./torrent.hpp"
#include "./ext.hpp"

#include <vector>
#include <variant>
#include <map>

namespace ttt {
	struct ToxClient;
} // ttt

namespace ttt::ext {

struct AnnounceInfoHashPackage {
	// i randomly decided you can sent at mose 4 info hashes per package.
	constexpr static size_t info_hashes_max_size = 4u;
	std::vector<std::variant<InfoHashV1, InfoHashV2>> info_hashes {};

	bool to(std::vector<uint8_t>& buff) const;
	bool from(const uint8_t* buff, const size_t buff_size);
};

class ToxExtAnnounce : public ToxClientExtension {
	public:
		ToxExtAnnounce(void) = default;

		void register_ext(ToxExt* toxext) override;
		void deregister_ext(ToxExt* toxext) override;

		bool announce_send(ToxExt* tox_ext, uint32_t friend_number, const AnnounceInfoHashPackage& aihp);

	public: // tox_client "interface"
		// ext support
		// if an entry exists, negotiantion is done
		std::map<uint32_t, bool> friend_compatiple {};

		float announce_interval = 30.f; // secounds
		struct FriendTimers {
			float timer = 0.f;
			std::vector<std::pair<float, Torrent>> torrent_timers{};
		};
		std::map<uint32_t, FriendTimers> friend_announce_timer {};

		void tick(void) override;

	public: // internal for callbacks
		struct UserData {
			ttt::ToxClient* tc;
			ToxExtAnnounce* tea;
		} ud{};
};

} // ttt::ext

