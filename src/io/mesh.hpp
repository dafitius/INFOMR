#ifndef QTTEST_MESH_H
#define QTTEST_MESH_H

#include "../lazy.hpp"
#include "obj_file.hpp"

#include <GL/gl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <set>
#include <span>
#include <utility>
#include <vector>

class Mesh
{
public:
	using Position = glm::vec3;
	using Normal = glm::vec3;
	using Face = std::vector<size_t>;

	struct Bounds
	{
		glm::vec3 min{std::numeric_limits<float>::max()};
		glm::vec3 max{std::numeric_limits<float>::lowest()};

		void expand(const glm::vec3& p)
		{
			min = glm::min(min, p);
			max = glm::max(max, p);
		}

		[[nodiscard]] glm::vec3 center() const
		{
			return (min + max) * 0.5f;
		}

		[[nodiscard]] float radius() const
		{
			return glm::length(max - min) * 0.5F;
		}

		[[nodiscard]] bool valid() const
		{
			return min.x <= max.x && min.y <= max.y && min.z <= max.z;
		}
	};

private:
	// non-mutable, create a new Mesh if you want to change these
	const std::vector<Position> positions_;
	const std::vector<Normal> normals_;
	const std::vector<Face> faces_;

	// mutable, lazily initialized
	mutable Lazy<Bounds> bounds_;
	mutable Lazy<Position> bary_center_;
	mutable Lazy<std::vector<std::uint32_t>> triangle_indices_;
	mutable Lazy<std::vector<std::uint32_t>> edge_indices_;
	mutable Lazy<size_t> skipped_faces_;

public:
	Mesh() = default;

	Mesh(std::vector<Position> positions, std::vector<Normal> normals, std::vector<Face> faces)
		: positions_(std::move(positions))
		, normals_(std::move(normals))
		, faces_(std::move(faces))
	{
	}

	template <typename... Args>
	static std::optional<Mesh> from_obj(Args&&... args)
	{
		std::optional<Mesh> result;
		auto ok = ObjFile::read(
			std::forward<Args>(args)...,
			[&result](auto pos, auto normal, auto faces)
			{
				auto convert = [](const auto& src)
				{
					std::vector<glm::vec3> dst;
					dst.reserve(src.size());
					std::ranges::transform(src, std::back_inserter(dst), [](const std::array<float, 3>& a) { return glm::vec3{a[0], a[1], a[2]}; });
					return dst;
				};
				result.emplace(Mesh{convert(pos), convert(normal), std::move(faces)});
			});
		if (ok)
			return result;
		return std::nullopt;
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
	std::span<const float> vertex_buffer() const
	{
		static_assert(sizeof(glm::vec3) == sizeof(GLfloat) * 3, "Platform doesn't support this directly.");
		return std::span{reinterpret_cast<const float*>(positions_.data()), positions_.size() * 3};
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
	Mesh translated(const glm::vec3& translation) const
	{
		auto new_positions = std::vector<Position>(positions_.size());
		std::ranges::transform(positions_, new_positions.begin(), [translation](auto& pos) { return pos + translation; });

		return Mesh{
			new_positions,
			normals_,
			faces_,
		};
	}

	[[nodiscard]]
	Mesh rotated(const glm::quat& rotation) const
	{
		auto new_positions = std::vector<Position>(positions_.size());
		std::ranges::transform(positions_, new_positions.begin(), [rotation](const auto& pos) { return rotation * pos; });

		auto new_normals = std::vector<Normal>(normals_.size());
		std::ranges::transform(normals_, new_normals.begin(), [rotation](const auto& normal) { return rotation * normal; });

		return Mesh{
			new_positions,
			new_normals,
			faces_,
		};
	}

	[[nodiscard]]
	Mesh scaled(const glm::vec3& scale) const
	{
		auto new_positions = std::vector<Position>(positions_.size());
		std::ranges::transform(positions_, new_positions.begin(), [scale](auto& pos) { return pos * scale; });

		return Mesh{
			new_positions,
			normals_,
			faces_,
		};
	}

	[[nodiscard]]
	Mesh remesh(const int target_vertex_count) const
	{
		int diff = target_vertex_count - positions_.size();

		auto new_positions = std::vector<Position>(target_vertex_count);
		std::ranges::copy(positions_, new_positions.begin());

		return Mesh{
			new_positions,
			new_normals,
			new_faces,
		};
	}

	[[nodiscard]]
	Mesh scaled(const float scale) const
	{
		return scaled(glm::vec3(scale, scale, scale));
	}

	[[nodiscard]] const Bounds& bounds() const
	{
		return bounds_.get([&] { return create_bounds(); });
	}

	[[nodiscard]] glm::vec3 bary_center() const
	{
		return bary_center_.get([&] { return calc_bary_center(); });
	}

	[[nodiscard]] std::span<const std::uint32_t> triangle_indices() const
	{
		auto& data = triangle_indices_.get([&] { return create_triangle_indices(); });
		return std::span{data.begin(), data.size()};
	}

	[[nodiscard]] std::span<const std::uint32_t> edge_indices() const
	{
		auto& data = edge_indices_.get([&] { return create_edge_indices(); });
		return std::span{data.begin(), data.size()};
	}

	[[nodiscard]] size_t num_skipped_faces() const
	{
		return skipped_faces_.get([&] { return calc_skipped_faces(); });
	}

private:
	[[nodiscard]] Bounds create_bounds() const
	{
		Bounds b{};
		for (const auto& p : positions_)
		{
			if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
				continue;
			b.expand(p);
		}
		return b.valid() ? b : Bounds{glm::vec3{0.F}, glm::vec3{0.F}};
	}

	[[nodiscard]] glm::vec3 calc_bary_center() const
	{
		auto x_iter = std::views::transform(positions_, [](auto& pos) { return pos.x; });
		auto y_iter = std::views::transform(positions_, [](auto& pos) { return pos.y; });
		auto z_iter = std::views::transform(positions_, [](auto& pos) { return pos.z; });

		double x_mean = std::accumulate(x_iter.begin(), x_iter.end(), 0.0) / static_cast<double>(positions_.size());
		double y_mean = std::accumulate(y_iter.begin(), y_iter.end(), 0.0) / static_cast<double>(positions_.size());
		double z_mean = std::accumulate(z_iter.begin(), z_iter.end(), 0.0) / static_cast<double>(positions_.size());
		return {x_mean, y_mean, z_mean};
	}

	[[nodiscard]] std::vector<std::uint32_t> create_triangle_indices() const
	{
		std::vector<std::uint32_t> out;
		out.reserve(faces_.size() * 3);
		for (const auto& face : faces_)
		{
			if (!is_valid_face(face))
				continue;
			// Fan triangulation, only correct for convex polygons
			for (size_t i = 1; i + 1 < face.size(); ++i)
			{
				out.push_back(static_cast<std::uint32_t>(face[0]));
				out.push_back(static_cast<std::uint32_t>(face[i]));
				out.push_back(static_cast<std::uint32_t>(face[i + 1]));
			}
		}
		return std::move(out);
	}

	[[nodiscard]] std::vector<std::uint32_t> create_edge_indices() const
	{
		std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
		edges.reserve(faces_.size() * 4);
		for (const auto& face : faces_)
		{
			if (!is_valid_face(face))
				continue;
			for (size_t i = 0; i < face.size(); ++i)
			{
				const auto a = static_cast<std::uint32_t>(face[i]);
				const auto b = static_cast<std::uint32_t>(face[(i + 1) % face.size()]);
				if (a != b)
					edges.emplace_back(std::min(a, b), std::max(a, b));
			}
		}
		std::ranges::sort(edges);
		const auto tail = std::ranges::unique(edges);
		edges.erase(tail.begin(), tail.end());

		std::vector<std::uint32_t> out;
		out.reserve(edges.size() * 2);
		for (const auto& [lo, hi] : edges)
		{
			out.push_back(lo);
			out.push_back(hi);
		}
		return std::move(out);
	}

	[[nodiscard]] size_t calc_skipped_faces() const
	{
		return static_cast<size_t>(std::ranges::count_if(faces_, [this](const Face& f) { return !is_valid_face(f); }));
	}

	[[nodiscard]] bool is_valid_face(const Face& face) const
	{
		const auto n = positions_.size();
		return face.size() >= 3 && std::ranges::all_of(face, [n](size_t i) { return i < n && i <= std::numeric_limits<std::uint32_t>::max(); });
	}
};

#endif // QTTEST_MESH_H
