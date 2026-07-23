#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace Tumbler::Nanite {

// Utility functions and helper classes for Nanite mesh processing.

struct Adjacency {
	static constexpr uint32_t inf = std::numeric_limits<uint32_t>::max();

	std::vector<uint32_t> adj;

	Adjacency(uint32_t num) : adj(num, inf) {}

	// Link vertex i and vertex j as adjacent.
	// 注意：仅适用于流形网格（每条边最多被两个三角形共享）。
	// 非流形网格会导致 assert(adj[*] == inf) 失败。
	// Asserts that both indices are in bounds and not already linked (Debug only).
	inline void link(uint32_t i, uint32_t j) {
		assert(i < adj.size() && "link: i out of bounds");
		assert(j < adj.size() && "link: j out of bounds");
		assert(adj[i] == inf && "link: adj[i] already linked");
		assert(adj[j] == inf && "link: adj[j] already linked");
		adj[i] = j;
		adj[j] = i;
	}
};

} // namespace Tumbler::Nanite
