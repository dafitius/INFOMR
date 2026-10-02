#ifndef QTTEST_MESH_H
#define QTTEST_MESH_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <format>
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

	struct Bounds
	{
		Position min;
		Position max;

		Position center() const
		{
			return {
				(min[0] + max[0]) * 0.5F,
				(min[1] + max[1]) * 0.5F,
				(min[2] + max[2]) * 0.5F,
			};
		}

		float radius() const
		{
			const auto c = Position{
				(max[0] - min[0]),
				(max[1] - min[1]),
				(max[2] - min[2]),
			};
			return rootn((sqrt(c[0]) + sqrt(c[1]) + sqrt(c[2])), 2) * 0.5F;
		}
	};

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
		assert(std::ranges::all_of(faces_, [](const auto& face) { return face.size() == 3; }) && "index_buffer() assumes all faces are triangles!");

		std::vector<size_t> buffer;
		buffer.reserve(faces_.size() * 3);

		for (const auto& face : faces_)
		{
			buffer.insert(buffer.end(), face.begin(), face.end());
		}

		return buffer;
	}

	[[nodiscard]]
	Bounds bounds() const
	{
		{
			constexpr float infinity = std::numeric_limits<float>::infinity();

			auto min = Mesh::Position{infinity, infinity, infinity};
			auto max = Mesh::Position{-infinity, -infinity, -infinity};

			for (const auto& [x, y, z] : this->positions_)
			{
				if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
				{
					continue;
				}

				min = Mesh::Position{std::min(min[0], x), std::min(min[1], y), std::min(min[2], z)};
				max = Mesh::Position{std::max(max[0], x), std::max(max[1], y), std::max(max[2], z)};
			}

			if (min[0] > max[0])
			{
				return {};
			}

			return {min, max};
		}
	}
};

#endif // QTTEST_MESH_H
