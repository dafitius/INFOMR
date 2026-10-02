#ifndef QTTEST_MESH_H
#define QTTEST_MESH_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <ranges>
#include <set>
#include <span>
#include <utility>
#include <vector>

class Mesh
{
public:
	using Position = std::array<float, 3>;
	using Normal = std::array<float, 3>;
	using Face = std::vector<size_t>;

private:
	std::vector<Position> positions_;
	std::vector<Normal> normals_;
	std::vector<Face> faces_;

public:
	Mesh() = default;

	Mesh(std::vector<Position> positions, std::vector<Normal> normals, std::vector<Face> faces)
		: positions_(std::move(positions))
		, normals_(std::move(normals))
		, faces_(std::move(faces))
	{
	}

	[[nodiscard]]
	size_t num_vertices() const
	{
		return positions_.size();
	}

	[[nodiscard]]
	size_t num_faces() const
	{
		return faces_.size();
	}

	/// @brief Check what type the faces are
	/// @return 0 = all faces tris, 1 = all faces quads, 2 = mixed/other
	[[nodiscard]]
	size_t face_type() const
	{
		auto tris = std::ranges::all_of(faces_, [](const auto& face) { return face.size() == 3; });
		if (tris)
		{
			return 0;
		}
		auto quads = std::ranges::all_of(faces_, [](const auto& face) { return face.size() == 4; });
		return quads ? 1 : 2;
	}

	[[nodiscard]]
	size_t num_edges() const
	{
		std::set<std::pair<size_t, size_t>> edges;

		for (const auto& face : faces_)
		{
			if (face.size() < 2)
				continue;

			for (size_t i = 0; i < face.size(); ++i)
			{
				const auto a = face[i];
				const auto b = face[(i + 1) % face.size()];

				if (a == b)
					continue;

				const auto [lo, hi] = std::minmax(a, b);
				edges.emplace(lo, hi);
			}
		}

		return edges.size();
	}

	[[nodiscard]]
	std::span<const Position> positions() const
	{
		return positions_;
	}

	[[nodiscard]]
	std::span<const Normal> normals() const
	{
		return normals_;
	}

	[[nodiscard]]
	std::span<const Face> faces() const
	{
		return faces_;
	}

	[[nodiscard]]
	std::vector<float> vertex_buffer() const
	{
		std::vector<float> buffer;
		buffer.reserve(positions_.size() * 3);

		for (const auto& position : positions_)
		{
			buffer.insert(buffer.end(), position.begin(), position.end());
		}

		return buffer;
	}

	[[nodiscard]]
	std::vector<size_t> index_buffer() const
	{
		std::vector<size_t> buffer;
		size_t faces_size = face_type();
		if (faces_size == 0)
		{
			buffer.reserve(faces_.size() * 3);
		}
		else
		{
			buffer.reserve(faces_.size() * 4);
		}

		for (const auto& face : faces_)
		{
			buffer.insert(buffer.end(), face.begin(), face.end());
		}

		return buffer;
	}
};

#endif // QTTEST_MESH_H
