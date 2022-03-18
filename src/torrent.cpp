#include "./torrent.hpp"

#include <cstdint>
#include <iostream>

bool Torrent::operator==(const Torrent& rhs) const {
	if (info_hash_v1) {
		if (rhs.info_hash_v1) {
			return *info_hash_v1 == *rhs.info_hash_v1;
		} else {
			return false;
		}
	}

	if (info_hash_v2) {
		if (rhs.info_hash_v2) {
			return *info_hash_v2 == *rhs.info_hash_v2;
		} else {
			return false;
		}
	}

	// invalid != invalid ?
	return false;
}

std::size_t std::hash<Torrent>::operator()(const Torrent& t) const noexcept {
	if (t.info_hash_v1) {
		std::size_t tmp_hash = 0;
		for (size_t i = 0; i < sizeof(std::size_t); i++) {
			tmp_hash |= t.info_hash_v1->data[i] << i*8;
		}
		return tmp_hash;
	} else if (t.info_hash_v2) {
		std::size_t tmp_hash = 0;
		for (size_t i = 0; i < sizeof(std::size_t); i++) {
			tmp_hash |= t.info_hash_v2->data[i] << i*8;
		}
		return tmp_hash;
	} else {
		return 0; // wtf
	}
}

std::ostream& operator<<(std::ostream& os, const Torrent& t) {
	if (t.info_hash_v1) {
		os << "v1:" << std::to_string(*t.info_hash_v1) << ";";
	}

	if (t.info_hash_v2) {
		os << "v2:" << std::to_string(*t.info_hash_v2) << ";";
	}

	return os;
}

