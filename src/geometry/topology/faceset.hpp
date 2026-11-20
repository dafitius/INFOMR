//
// Created by dafitius on 22-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_FACESET_HPP
#define MULTIMEDIARETRIEVAL_FACESET_HPP

#include "../face_list.hpp"
#include "common.hpp"
#include "halfedge.hpp"
#include <ranges>

/// Wrapper for FaceList to keep data and logic separated
class FaceSet {
public:
	enum class FaceType : uint8_t { TRI = 0, QUAD = 1, MIXED = 2 };

	struct EdgeOccurrence {
		FaceList::Index face;
		FaceList::Index from;
		FaceList::Index to;
	};

	struct FaceNeighbor {
		FaceList::Index face;
		bool flipRelative;
	};

	using FaceAdjacency = std::vector<std::vector<FaceList::Index>>;
	using EdgeIncidenceMap = std::unordered_map<FaceList::Edge, std::vector<EdgeOccurrence>, DirectedEdgeKeyHash>;

private:
	FaceList face_list_;

	mutable Lazy<FaceType> face_type_;

public:
	FaceSet() = default;

	explicit FaceSet(const FaceList& faceList)
		: face_list_(faceList) {}

	const FaceList& list() const {
		return face_list_;
	}

	FaceList& mutableList() {
		return face_list_;
	}

	FaceType faceType() const {
		return face_type_.getOrCreate([&] { return calcFaceType(); });
	}

	[[nodiscard]] EdgeIncidenceMap edgeIncidence() const {
		EdgeIncidenceMap result;
		result.reserve(face_list_.indices().size());

		for (const auto& [i, face] : face_list_.all() | std::views::enumerate) {
			for (FaceList::Index face_i = 0; face_i < face.size(); ++face_i) {
				const FaceList::Index from = face[face_i];
				const FaceList::Index to = face[(face_i + 1) % face.size()];

				if (from == to)
					continue;

				const FaceList::Edge key{.a = from, .b = to};
				result[key.canonize()].push_back(EdgeOccurrence{.face = static_cast<size_t>(i), .from = from, .to = to});
			}
		}
		return result;
	}

	[[nodiscard]] FaceAdjacency faceAdjacency() const {
		const auto incidence = edgeIncidence();
		FaceAdjacency result(face_list_.size());

		for (const auto& occurrences : incidence | std::views::values) {
			if (occurrences.size() != 2)
				continue;

			const auto& a = occurrences[0];
			const auto& b = occurrences[1];

			if (a.face == b.face)
				continue;

			result[a.face].push_back(b.face);
			result[b.face].push_back(a.face);
		}

		return result;
	}

private:
	FaceType calcFaceType() const {
		if (face_list_.empty()) {
			return FaceType::MIXED;
		}
		auto faceSize = face_list_[0].size();
		const auto equal = std::ranges::all_of(face_list_.all(), [&faceSize](const auto& face) { return face.size() == faceSize; });
		if (equal && faceSize == 3) {
			return FaceType::TRI;
		}
		if (equal && faceSize == 4) {
			return FaceType::QUAD;
		}
		return FaceType::MIXED;
	}
};

#endif // MULTIMEDIARETRIEVAL_FACESET_HPP
