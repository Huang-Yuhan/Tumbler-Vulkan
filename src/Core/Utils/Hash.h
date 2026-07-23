#pragma once

#include <bit>
#include <cstdint>
#include <glm/glm.hpp>
#include <initializer_list>

namespace Tumbler {

inline uint32_t MurmurFinalize32(uint32_t Hash) {
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

inline uint64_t MurmurFinalize64(uint64_t Hash) {
	Hash ^= Hash >> 33;
	Hash *= 0xff51afd7ed558ccdull;
	Hash ^= Hash >> 33;
	Hash *= 0xc4ceb9fe1a85ec53ull;
	Hash ^= Hash >> 33;
	return Hash;
}

inline uint32_t Murmur32(std::initializer_list<uint32_t> InitList) {
	uint32_t Hash = 0;
	for (auto Element : InitList) {
		Element *= 0xcc9e2d51;
		Element = (Element << 15) | (Element >> (32 - 15));
		Element *= 0x1b873593;

		Hash ^= Element;
		Hash = (Hash << 13) | (Hash >> (32 - 13));
		Hash = Hash * 5 + 0xe6546b64;
	}
	return MurmurFinalize32(Hash);
}

inline uint64_t Murmur64(std::initializer_list<uint64_t> InitList) {
	uint64_t Hash = 0;
	for (auto Element : InitList) {
		Element *= 0x87c37b91114253d5ull;
		Element = (Element << 31) | (Element >> (64 - 31));
		Element *= 0x4cf5ad432745937full;

		Hash ^= Element;
		Hash = (Hash << 27) | (Hash >> (64 - 27));
		Hash = Hash * 5 + 0x52dce729;
	}
	return MurmurFinalize64(Hash);
}

inline uint32_t HashPosition(const glm::vec3& Position) {
	uint32_t xi = std::bit_cast<uint32_t>(Position.x);
	uint32_t yi = std::bit_cast<uint32_t>(Position.y);
	uint32_t zi = std::bit_cast<uint32_t>(Position.z);
	return Murmur32({
		Position.x == 0.0f ? 0u : xi,
		Position.y == 0.0f ? 0u : yi,
		Position.z == 0.0f ? 0u : zi,
	});
}

} // namespace Tumbler
