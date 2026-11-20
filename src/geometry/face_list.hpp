#ifndef MULTIMEDIARETRIEVAL_FACE_LIST_H
#define MULTIMEDIARETRIEVAL_FACE_LIST_H

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <vector>

/// Simple mutable container for n-edge polygons.
class FaceList {
public:
	using Index = size_t;
	using Face = std::span<const Index>;

	struct Edge {
		Index a;
		Index b;
		bool operator==(const Edge&) const = default;

		Edge canonize() const {
			return {.a = std::min(a, b), .b = std::max(a, b)};
		}
	};

	FaceList() = default;

	// Optional pre-allocate space for the given number of faces and indices
	void reserve(const size_t faces, const size_t indices) {
		offsets_.reserve(faces + 1);
		indices_.reserve(indices);
	}

	// Appends one complete face, faces.push_back(std::array<uint32_t, 3>{0, 1, 2})
	void push_back(const Face face) {
		indices_.insert(indices_.end(), face.begin(), face.end());
		offsets_.push_back(indices_.size());
	}

	// Appends all inserted vertices as a face
	template <std::convertible_to<std::size_t>... Ts>
		requires(sizeof...(Ts) > 0)
	void push_back(Ts... vertices) {
		(indices_.push_back(static_cast<std::size_t>(vertices)), ...);
		offsets_.push_back(indices_.size());
	}

	template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_value_t<R>, Index>
	void push_range(R&& range) {
		for (const auto v : range) {
			indices_.push_back(static_cast<Index>(v));
		}
		offsets_.push_back(indices_.size());
	}

	// Adds one vertex index to the face being built, call end_face() when it is complete
	void push_index(const Index index) {
		indices_.push_back(index);
	}

	void end_face() {
		offsets_.push_back(indices_.size());
	}

	// Number of faces in the list
	[[nodiscard]] size_t size() const noexcept {
		return offsets_.size() - 1;
	}

	[[nodiscard]] bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] std::optional<Face> get(std::size_t i) const noexcept {
		if (i + 1 >= offsets_.size()) {
			return std::nullopt;
		}

		const auto start = offsets_[i];
		const auto end = offsets_[i + 1];

		if (end > indices_.size() || start > end) {
			return std::nullopt;
		}
		return Face(indices_.begin() + start, indices_.begin() + end);
	}

	// Vertex indices of face i (no bounds check)
	[[nodiscard]] Face operator[](const size_t i) const {
		return *this->get(i);
	}

	// View over all faces, usable with range-for and std::ranges algorithms.
	[[nodiscard]] auto all() const {
		return std::views::iota(size_t{0}, size()) | std::views::transform([this](const size_t i) { return *this->get(i); });
	}

	// All indices of all faces back to back, without face boundaries
	[[nodiscard]] std::span<const Index> indices() const noexcept {
		return indices_;
	}

	void remove(const std::size_t faceIndex) {
		if (faceIndex >= size()) {
			return;
		}

		const auto start = offsets_[faceIndex];
		const auto end = offsets_[faceIndex + 1];
		const auto removed = end - start;

		indices_.erase(indices_.begin() + start, indices_.begin() + end);

		offsets_.erase(offsets_.begin() + faceIndex + 1);
		for (std::size_t i = faceIndex + 1; i < offsets_.size(); ++i) {
			offsets_[i] -= removed;
		}
	}

	void replace(const std::size_t faceIndex, const Index from, const Index to) {
		if (faceIndex >= size()) {
			return;
		}

		const auto start = offsets_[faceIndex];
		const auto end = offsets_[faceIndex + 1];

		for (auto i = start; i < end; ++i) {
			if (indices_[i] == from) {
				indices_[i] = to;
				return;
			}
		}
	}

	static bool isValid(const Face& face, const size_t numVertices) {
		return face.size() >= 3 && std::ranges::all_of(face, [numVertices](const std::uint32_t i) { return i < numVertices; });
	}

	// Reverse the face order
	void reverseFace(const std::size_t faceIndex) noexcept {
		assert(faceIndex + 1 < offsets_.size());
		const auto startIndex = static_cast<ptrdiff_t>(offsets_[faceIndex]);
		const auto endIndex = static_cast<ptrdiff_t>(offsets_[faceIndex + 1]);
		std::reverse(indices_.begin() + startIndex, indices_.begin() + endIndex);
	}

private:
	std::vector<Index> indices_;
	std::vector<Index> offsets_{0};
};

#endif // MULTIMEDIARETRIEVAL_FACE_LIST_H
