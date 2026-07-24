#pragma once

#include <cstdint>

namespace Tumbler::Utils {

	struct Range {
	uint32_t start;
	uint32_t end;
	Range(uint32_t s, uint32_t e) : start(s), end(e) {}
	uint32_t size() const { return end - start; }
	bool operator<(const Range& other) const { return start < other.start; }
	};

} // namespace Tumbler::Utils
