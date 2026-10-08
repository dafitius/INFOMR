#ifndef MULTIMEDIARETRIEVAL_FACE_LIST_H
#define MULTIMEDIARETRIEVAL_FACE_LIST_H

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
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

	// Vertex indices of face i (no bounds check)
	[[nodiscard]] Face operator[](const size_t i) const
	{
		return Face{indices_.data() + offsets_[i], indices_.data() + offsets_[i + 1]};
	}

	// View over all faces, usable with range-for and std::ranges algorithms.
	[[nodiscard]] auto all() const
	{
		return std::views::iota(size_t{0}, size()) | std::views::transform([this](const size_t i) { return (*this)[i]; });
	}

	// All indices of all faces back to back, without face boundaries
	[[nodiscard]] std::span<const Index> indices() const noexcept
	{
		return indices_;
	}

private:
	std::vector<Index> indices_;
	std::vector<Index> offsets_{0};
};

#endif // MULTIMEDIARETRIEVAL_FACE_LIST_H
