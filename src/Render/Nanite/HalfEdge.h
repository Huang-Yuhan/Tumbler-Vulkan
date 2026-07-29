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

	// ---- Direct adjacency array builder ----
	// After all AddEdge calls, builds a compact Direct[] array where
	// Direct[edgeIndex] = pairedEdgeIndex (-1 = no match, -2 = >2 matches).
	template <PositionProvider GetPosition>
	void BuildDirectAdjacency(std::vector<int32_t>& outDirect, uint32_t numEdges,
	                          GetPosition&& GetPos) const {
		outDirect.assign(numEdges, -1);

		for (uint32_t edgeIndex = 0; edgeIndex < numEdges; ++edgeIndex) {
			int32_t adjCount = 0;
			int32_t adjIndex = -1;
			ForAllMatching(edgeIndex, GetPos,
				[&](uint32_t /*a*/, uint32_t b) {
					adjIndex = static_cast<int32_t>(b);
					++adjCount;
				});
			outDirect[edgeIndex] = (adjCount == 1) ? adjIndex : (adjCount > 1 ? -2 : -1);
		}
	}

	// Iterate all unique matched pairs where first < second
	template <typename EdgePairCallback>
	static void ForAllPairs(const std::vector<int32_t>& direct, uint32_t numEdges,
	                        EdgePairCallback&& callback) {
		for (uint32_t e = 0; e < numEdges; ++e) {
			int32_t adj = direct[e];
			if (adj >= 0 && static_cast<uint32_t>(adj) > e) {
				callback(e, static_cast<uint32_t>(adj));
			}
		}
	}

	// Callback for a specific edge: func(edgeIndex, adjEdgeIndex) if matched
	template <typename EdgePairCallback>
	static void ForEachAdjacency(const std::vector<int32_t>& direct, uint32_t edgeIndex,
	                             EdgePairCallback&& callback) {
		int32_t adj = direct[edgeIndex];
		if (adj >= 0) {
			callback(edgeIndex, static_cast<uint32_t>(adj));
		}
	}
};

} // namespace Tumbler::Nanite
