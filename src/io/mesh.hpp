#ifndef QTTEST_MESH_H
#define QTTEST_MESH_H

#include "../lazy.hpp"
#include "halfedge.hpp"
#include "face_list.hpp"
#include "obj_file.hpp"

#include <GL/gl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <execution>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <utility>
#include <vector>

class Mesh
{
public:
	using Position = glm::vec3;
	using Normal = glm::vec3;
	using Face = FaceList::Face;

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

	struct Edge
	{
		std::uint32_t a;
		std::uint32_t b;

		bool operator==(const Edge&) const = default;
		auto operator<=>(const Edge&) const = default;
	};

	static_assert(sizeof(Edge) == sizeof(std::uint32_t) * 2, "Edge has to be two u32!!");

private:
	// non-mutable, create a new Mesh if you want to change these
	const std::vector<Position> positions_;
	const std::vector<Normal> normals_;
	const FaceList faces_;

	// mutable, lazily initialized
	mutable Lazy<Bounds> bounds_;
	mutable Lazy<Position> bary_center_;
	mutable Lazy<std::vector<std::uint32_t>> triangle_indices_;
	mutable Lazy<std::vector<Edge>> edges_;
	mutable Lazy<size_t> skipped_faces_;
	mutable Lazy<HalfEdge::Topology> topology_;

public:
	Mesh() = default;

	Mesh(std::vector<Position> positions, std::vector<Normal> normals, FaceList faces)
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
		auto tris = std::ranges::all_of(faces_.all(), [](const Face face) { return face.size() == 3; });
		if (tris)
		{
			return 0;
		}
		auto quads = std::ranges::all_of(faces_.all(), [](const Face face) { return face.size() == 4; });
		return quads ? 1 : 2;
	}

	[[nodiscard]]
	size_t num_edges() const
	{
		auto& data = edges_.get_or_create([&] { return create_edges(); });
		return data.size();
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
	const FaceList& faces() const
	{
		return faces_;
	}

	[[nodiscard]]
	std::span<const Edge> edges() const
	{
		auto& edges = edges_.get_or_create([&] { return create_edges(); });
		return edges;
	}

	[[nodiscard]]
	std::vector<Edge> boundary_edges() const
	{
		const auto hes = topology().halfedges();

		std::vector<Edge> out;
		for (const auto& he : hes)
		{
			if (he.opposite.has_value())
				continue;

			const auto a = static_cast<std::uint32_t>(he.vertex);
			const auto b = static_cast<std::uint32_t>(hes[he.next].vertex);
			out.push_back(Edge{a, b});
		}
		return out;
	}

	[[nodiscard]]
	std::span<const float> vertex_position_buffer() const
	{
		static_assert(sizeof(glm::vec3) == sizeof(GLfloat) * 3, "Platform doesn't support this directly.");
		return std::span{reinterpret_cast<const float*>(positions_.data()), positions_.size() * 3};
	}

	[[nodiscard]]
	std::span<const float> vertex_normal_buffer() const
	{
		static_assert(sizeof(glm::vec3) == sizeof(GLfloat) * 3, "Platform doesn't support this directly.");
		return std::span{reinterpret_cast<const float*>(normals_.data()), normals_.size() * 3};
	}

	[[nodiscard]]
	std::span<const std::size_t> index_buffer() const
	{
		assert(std::ranges::all_of(faces_.all(), [](const auto& face) { return face.size() == 3; }) && "index_buffer() assumes all faces are triangles!");

		std::vector<size_t> buffer;
		buffer.reserve(faces_.size() * 3);

		for (const Face face : faces_.all())
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
	Mesh filled_holes() const
	{
		auto positions = positions_;
		auto faces = faces_;

		for (const auto& loop : boundary_loops())
		{
			std::vector<Position> boundary_positions;
			boundary_positions.reserve(loop.size());

			for (const auto vertex : loop)
				boundary_positions.push_back(positions_[vertex]);

			const auto center = calc_bary_center(boundary_positions);

			const auto center_index = positions.size();
			positions.push_back(center);

			auto closed_loop = loop;
			closed_loop.push_back(loop.front());
			for (const auto window : closed_loop | std::views::slide(2))
			{
				faces.push_back(window[0], window[1], center_index);
			}
		}

		return Mesh{std::move(positions), normals_, std::move(faces)};
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
	Mesh with_consistent_orientations() const
	{
		auto new_faces = faces_;

		const auto& topo = topology();
		const auto hes = topo.halfedges();

		std::vector visitedFaces(new_faces.size(), false);
		std::vector<HalfEdge::FaceRef> stack;
		stack.reserve(new_faces.size());

		auto sameDirectionInFace = [&](const HalfEdge::FaceRef f, const HalfEdge::EdgeKey edge) -> std::optional<bool>
		{
			const auto face = new_faces[f];

			for (std::size_t i = 0; i < face.size(); ++i)
			{
				const auto vertex0 = face[i];
				const auto vertex1 = face[(i + 1) % face.size()];

				if (vertex0 == edge.a && vertex1 == edge.b)
					return true;
				if (vertex0 == edge.b && vertex1 == edge.a)
					return false;
			}
			return std::nullopt;
		};

		HalfEdge::FaceRef seed = 0;
		visitedFaces[seed] = true;
		stack.push_back(seed);

		while (!stack.empty())
		{
			const auto f = stack.back();
			stack.pop_back();

			const auto fh = topo.get_halfedge_from_face(f);
			if (!fh)
				continue;

			auto h = fh->next;
			const auto start = fh->next;

			while (true)
			{
				const auto& he = hes[h];
				if (he.opposite.has_value())
				{
					const auto other = hes[*he.opposite].face;
					if (!visitedFaces[other])
					{
						const auto a = he.vertex;
						const auto b = hes[he.next].vertex;

						// If the neighbor uses the same edge in the same direction,
						if (sameDirectionInFace(other, {.a = a, .b = b}) == true)
						{
							new_faces.reverseFace(other);
						}

						visitedFaces[other] = true;
						stack.push_back(other);
					}
				}

				h = he.next;
				if (h == start)
					break;
			}
		}

		return Mesh{
			positions_,
			normals_,
			std::move(new_faces),
		};
	}

	[[nodiscard]] const Bounds& bounds() const
	{
		return bounds_.get_or_create([&] { return create_bounds(); });
	}

	[[nodiscard]] glm::vec3 bary_center() const
	{
		return bary_center_.get_or_create([&] { return calc_bary_center(positions_); });
	}

	[[nodiscard]] std::span<const std::uint32_t> triangle_indices() const
	{
		const auto& data = triangle_indices_.get_or_create([&] { return create_triangle_indices(); });
		return std::span{data.begin(), data.size()};
	}

	[[nodiscard]] std::span<const std::uint32_t> edge_indices() const
	{
		const auto& data = edges_.get_or_create([&] { return create_edges(); });
		return std::span{reinterpret_cast<const uint32_t*>(data.data()), data.size() * 2};
	}

	[[nodiscard]] size_t num_skipped_faces() const
	{
		return skipped_faces_.get_or_create([&] { return calc_skipped_faces(); });
	}


private:
	[[nodiscard]] const HalfEdge::Topology& topology() const
	{
		return topology_.get_or_create([&] { return HalfEdge::Topology::build(faces_, positions_.size()); });
	}

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

	static glm::vec3 calc_bary_center(std::span<const glm::vec3> points)
	{
		auto x_iter = std::views::transform(points, [](auto& pos) { return pos.x; });
		auto y_iter = std::views::transform(points, [](auto& pos) { return pos.y; });
		auto z_iter = std::views::transform(points, [](auto& pos) { return pos.z; });

		double x_mean = std::accumulate(x_iter.begin(), x_iter.end(), 0.0) / static_cast<double>(points.size());
		double y_mean = std::accumulate(y_iter.begin(), y_iter.end(), 0.0) / static_cast<double>(points.size());
		double z_mean = std::accumulate(z_iter.begin(), z_iter.end(), 0.0) / static_cast<double>(points.size());
		return {x_mean, y_mean, z_mean};
	}

	[[nodiscard]] std::vector<std::uint32_t> create_triangle_indices() const
	{
		std::vector<std::uint32_t> out;
		out.reserve(faces_.size() * 3);
		for (const auto [i, face] : faces_.all() | std::views::enumerate)
		{
			if (!FaceList::isValid(face, positions_.size()))
				continue;
			// Fan triangulation, only correct for convex polygons
			for (size_t i = 1; i + 1 < face.size(); ++i)
			{
				out.push_back(face[0]);
				out.push_back(face[i]);
				out.push_back(face[i + 1]);
			}
		}
		return out;
	}

	[[nodiscard]]
	std::vector<Edge> create_edges() const
	{
		std::vector<Edge> occurrences;
		occurrences.reserve(faces_.size() * 4);

		for (const auto& face : faces_.all())
		{
			if (!FaceList::isValid(face, positions_.size()))
				continue;

			for (std::size_t i = 0; i < face.size(); ++i)
			{
				const auto a = face[i];
				const auto b = face[(i + 1) % face.size()];
				if (a == b) continue;
				occurrences.emplace_back(std::min(a, b), std::max(a, b));
			}
		}

		std::ranges::sort(occurrences);

		std::vector<Edge> edges;
		edges.reserve(occurrences.size());

		for (std::size_t i = 0; i < occurrences.size();)
		{
			const auto edge = occurrences[i];

			std::size_t j = i + 1;
			while (j < occurrences.size() && occurrences[j] == edge) {
				++j;
			}
			edges.push_back(edge);
			i = j;
		}
		return edges;
	}

	[[nodiscard]] size_t calc_skipped_faces() const
	{
		return static_cast<size_t>(std::ranges::count_if(faces_.all(), [this](const Face f) { return !FaceList::isValid(f, positions_.size()); }));
	}

	[[nodiscard]] std::vector<std::vector<std::size_t>> boundary_loops() const
	{
		const auto& topo = topology();
		const auto& hes = topo.halfedges();

		auto next_boundary = [&](HalfEdge::HalfedgeRef h) -> std::optional<HalfEdge::HalfedgeRef>
		{
			auto g = hes[h].next;
			for (std::size_t guard = 0; hes[g].opposite.has_value() && guard < hes.size(); ++guard)
				g = hes[*hes[g].opposite].next;
			return hes[g].opposite.has_value() ? std::nullopt : std::make_optional(g);
		};

		std::vector seen(hes.size(), false);
		std::vector<std::vector<std::size_t>> loops;

		for (HalfEdge::HalfedgeRef start = 0; start < hes.size(); ++start)
		{
			if (hes[start].opposite.has_value() || seen[start])
				continue;

			std::vector<std::size_t> loop;
			auto h = std::make_optional(start);
			bool closed = false;

			while (h && !seen[h.value()])
			{
				const auto current = h.value();
				seen[current] = true;
				loop.push_back(hes[current].vertex);
				h = next_boundary(current);
				closed = h && h.value() == start;
			}

			if (closed && loop.size() >= 3)
				loops.push_back(std::move(loop));
		}

		return loops;
	}
};

#endif // QTTEST_MESH_H
