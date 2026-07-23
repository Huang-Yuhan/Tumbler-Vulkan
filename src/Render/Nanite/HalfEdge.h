#pragma once

#include <concepts>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include "Core/Utils/Hash.h"

namespace Tumbler::Nanite {

// Half-edge representation of a triangle mesh.
// Provides O(1) adjacency queries for mesh traversal, cluster partitioning,
// and edge-collapse simplification.
//
// Standard half-edge layout:
//   opposite edge ──→
//          ←── pair
//   vertex ──→ half-edge ──→ face
//                    │
//                    └──→ next half-edge (CCW around face)

struct HalfEdge {
	uint32_t from;
	uint32_t to;
};

// PositionProvider: 给定顶点下标(uint32_t)，返回该顶点的世界空间坐标(glm::vec3)。
// AddEdge / ForAllMatching 通过此概念抽象顶点数据的存储方式。
template <typename T>
concept PositionProvider = requires(T&& f, uint32_t i) {
	{ f(i) } -> std::same_as<glm::vec3>;
};

// EdgeCallback: 每找到一对匹配的半边时调用，参数为 (fromIndex, toIndex)。
// 两个半边共享同一条边，方向相反。
template <typename T>
concept EdgeCallback = requires(T&& f, uint32_t a, uint32_t b) {
	{ f(a, b) } -> std::same_as<void>;
};

class HalfEdgeHashTable {
	// 如果两个顶点的位置相同，则认为它们是相同的顶点
private:
	/*
	* @param i 是indices中的下标，代表了indices[i]这个顶点
	* @brief 组成三角形的3个半边的起始顶点一定在indices中是连续的
	* @return 返回这个三角形中以indices[i]为起点的半边的终点在indices中的下标,假设对于半边(indices[i],indices[j]),这里返回的是j
	*/
	inline static uint32_t cycle3(uint32_t i) {
		uint32_t div3 = i / 3;
		uint32_t mod3 = i % 3;
		return div3 * 3 + (mod3 + 1) % 3;
	}

	std::unordered_multimap<uint32_t, uint32_t> map;

public:
	template <PositionProvider GetPosition>
	void AddEdge(uint32_t from_index_indices_array, GetPosition&& GetPos) {
		glm::vec3 p1 = GetPos(from_index_indices_array);
		glm::vec3 p2 = GetPos(cycle3(from_index_indices_array));

		uint32_t hash0 = HashPosition(p1);
		uint32_t hash1 = HashPosition(p2);

		auto Key = Murmur32({hash0, hash1});
		map.emplace(Key, from_index_indices_array);
	}

	template <PositionProvider GetPosition, EdgeCallback FuncType>
	void ForAllMatching(uint32_t idx, GetPosition&& GetPos, FuncType&& Func) const {
		glm::vec3 p1 = GetPos(idx);
		glm::vec3 p2 = GetPos(cycle3(idx));

		uint32_t hash0 = HashPosition(p1);
		uint32_t hash1 = HashPosition(p2);

		// Look up the opposite direction: p2→p1
		auto Key = Murmur32({hash1, hash0});

		auto [begin, end] = map.equal_range(Key);
		for (auto it = begin; it != end; it++) {
                    auto otherIndex = it->second;
                    if (p2 == GetPos(otherIndex) && p1 == GetPos(cycle3(otherIndex))) {
                        Func(idx, otherIndex);
			}
		}
	}
};

} // namespace Tumbler::Nanite
