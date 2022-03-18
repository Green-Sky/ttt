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
		virtual void register_ext(ToxExt* toxext) = 0;
		void negotiate_connection(uint32_t friend_number);
};

} // ttt::ext

