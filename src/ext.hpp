#pragma once

#include <cstdint>

extern "C" {
	struct ToxExt;
	struct ToxExtExtension;
}

namespace ttt::ext {

class ToxClientExtension {
	protected:
		::ToxExtExtension* _tee = nullptr;
	public:
		// TODO: replace with ctr/dtr
		virtual void register_ext(ToxExt* toxext) = 0;
		virtual void deregister_ext(ToxExt* toxext) = 0;
		void negotiate_connection(uint32_t friend_number);

		virtual void tick(void) {};
};

} // ttt::ext

