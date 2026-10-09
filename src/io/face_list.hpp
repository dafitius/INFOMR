#ifndef MULTIMEDIARETRIEVAL_FACE_LIST_H
#define MULTIMEDIARETRIEVAL_FACE_LIST_H

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <vector>


class FaceList
{
public:
	using Index = size_t;
	using Face = std::span<const Index>;

	FaceList() = default;

	// Optional pre-allocate space for the given number of faces and indices
	void reserve(const size_t faces, const size_t indices)
	{
		offsets_.reserve(faces + 1);
		indices_.reserve(indices);
	}

	// Appends one complete face, faces.push_back(std::array<uint32_t, 3>{0, 1, 2})
	void push_back(const Face face)
	{
		indices_.insert(indices_.end(), face.begin(), face.end());
		offsets_.push_back(indices_.size());
	}

	// Appends all inserted vertices as a face
	template <std::convertible_to<std::size_t>... Ts>
		requires (sizeof...(Ts) > 0)
	void push_back(Ts... vertices)
	{
		(indices_.push_back(static_cast<std::size_t>(vertices)), ...);
		offsets_.push_back(indices_.size());
	}

	// Adds one vertex index to the face being built, call end_face() when it is complete
	void push_index(const Index index)
	{
		indices_.push_back(index);
	}

	void end_face()
	{
		offsets_.push_back(indices_.size());
	}

	// Number of faces in the list
	[[nodiscard]] size_t size() const noexcept
	{
		return offsets_.size() - 1;
	}

	[[nodiscard]] bool empty() const noexcept
	{
		return size() == 0;
	}

	[[nodiscard]] std::optional<Face> get(std::size_t i) const noexcept
	{
		if (i + 1 >= offsets_.size()) return std::nullopt;

		const auto start = offsets_[i];
		const auto end = offsets_[i + 1];

		if (end > indices_.size() || start > end) return std::nullopt;
		return Face(indices_.begin() + start, indices_.begin() + end);
	}
	// Vertex indices of face i (no bounds check)
	[[nodiscard]] Face operator[](const size_t i) const
	{
		return *this->get(i);
	}

	// View over all faces, usable with range-for and std::ranges algorithms.
	[[nodiscard]] auto all() const
	{
		return std::views::iota(size_t{0}, size()) | std::views::transform([this](const size_t i) { return *this->get(i); });
	}

	// All indices of all faces back to back, without face boundaries
	[[nodiscard]] std::span<const Index> indices() const noexcept
	{
		return indices_;
	}

	static bool isValid(const Face& face, const size_t num_vertices)
	{
		return face.size() >= 3 && std::ranges::all_of(face, [num_vertices](const std::uint32_t i) { return i < num_vertices; });
	}

	// Reverse the face order
	void reverseFace(const std::size_t face_index) noexcept
	{
		assert(face_index + 1 < offsets_.size());
		const auto start_index = static_cast<ptrdiff_t>(offsets_[face_index]);
		const auto end_index = static_cast<ptrdiff_t>(offsets_[face_index + 1]);
		std::reverse(indices_.begin() + start_index, indices_.begin() + end_index);
	}

private:
	std::vector<Index> indices_;
	std::vector<Index> offsets_{0};
};

#endif // MULTIMEDIARETRIEVAL_FACE_LIST_H
