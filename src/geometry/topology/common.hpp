//
// Created by dafitius on 22-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_COMMON_HPP
#define MULTIMEDIARETRIEVAL_COMMON_HPP

#include <algorithm>
#include <cstdint>

template <typename T>
concept EdgeKeyLike = requires(const T& key) {
	key.a;
	key.b;
};

struct DirectedEdgeKeyHash {
	template <EdgeKeyLike T>
	[[nodiscard]] std::size_t operator()(const T& edge) const noexcept {
		const auto h1 = std::hash<FaceList::Index>{}(edge.a);
		const auto h2 = std::hash<FaceList::Index>{}(edge.b);
		return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(h1) << static_cast<uint32_t>(32)) | h2);
	}
};

struct UndirectedEdgeKeyHash {
	template <EdgeKeyLike T>
	std::size_t operator()(const T& k) const noexcept {
		const auto low = std::min(k.a, k.b);
		const auto high = std::max(k.a, k.b);
		return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(low) << static_cast<uint32_t>(32)) | high);
	}
};


#endif // MULTIMEDIARETRIEVAL_COMMON_HPP
