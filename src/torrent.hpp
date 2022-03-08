#pragma once

#include <array>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <cctype>
#include <cassert>

static uint8_t _single_hex_char_to_value(const char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - ('a' - 10);
	} else {
		assert(false && "meh invalid char");
	}
}

template<size_t bytes>
struct Hash {
	std::array<uint8_t, bytes> data {};

	Hash(void) = default;
	explicit Hash(const std::string& str) {
		assert(str.size() == bytes*2);

		for (size_t i = 0; i < bytes; i++) {
			data[i] =
				_single_hex_char_to_value(std::tolower(static_cast<unsigned char>(str.at(i*2)))) << 4 |
				_single_hex_char_to_value(std::tolower(static_cast<unsigned char>(str.at(i*2+1))))
			;
		}
	}

	bool operator==(const Hash& rhs) const {
		return data == rhs.data;
	}
};

// sha1
using InfoHashV1 = Hash<20>;

// sha256
using InfoHashV2 = Hash<32>;

namespace std {
	inline std::string to_string(const std::vector<uint8_t>& arr) {
		static const char hex_lut[] {
			'0', '1', '2', '3',
			'4', '5', '6', '7',
			'8', '9', 'a', 'b',
			'c', 'd', 'e', 'f',
		};

		std::string tmp_str(arr.size()*2, ' ');

		for (size_t i = 0; i < arr.size(); i++) {
			tmp_str.at(i*2) = hex_lut[(arr.at(i) & 0xf0) >> 4];
			tmp_str.at(i*2+1) = hex_lut[arr.at(i) & 0x0f];
		}

		return tmp_str;
	}

	template<size_t bytes>
	std::string to_string(const std::array<uint8_t, bytes>& arr) {
		static const char hex_lut[] {
			'0', '1', '2', '3',
			'4', '5', '6', '7',
			'8', '9', 'a', 'b',
			'c', 'd', 'e', 'f',
		};

		std::string tmp_str(bytes*2, ' ');

		for (size_t i = 0; i < bytes; i++) {
			tmp_str.at(i*2) = hex_lut[(arr.at(i) & 0xf0) >> 4];
			tmp_str.at(i*2+1) = hex_lut[arr.at(i) & 0x0f];
		}

		return tmp_str;
	}

	template<size_t bytes>
	std::string to_string(const Hash<bytes>& hash) {
		return to_string(hash.data);
	}
} // std


// contains info hash
struct Torrent {
	std::optional<InfoHashV1> info_hash_v1;
	std::optional<InfoHashV2> info_hash_v2;

	bool valid(void) const {
		return info_hash_v1 || info_hash_v2;
	}

	bool operator==(const Torrent& rhs) const;
};

template<>
struct std::hash<Torrent> {
	std::size_t operator()(const Torrent& t) const noexcept;
};

