#pragma once

#include <cstdint>

namespace Tumbler::Utils {

	struct Range {
	int32_t start = 0;
	int32_t end   = 0;
	Range() = default;
	Range(int32_t s, int32_t e) : start(s), end(e) {}
	int32_t size() const { return end - start; }
	bool operator<(const Range& other) const { return start < other.start; }
	};

} // namespace Tumbler::Utils
