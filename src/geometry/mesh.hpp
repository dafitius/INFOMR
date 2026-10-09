#ifndef MR_MESH_H
#define MR_MESH_H

#include "../io/obj_file.hpp"
#include "../lazy.hpp"
#include "face_list.hpp"
#include "halfedge.hpp"
#include "ops/ops.h"
#include "pca.hpp"

#include <GL/gl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
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
	mutable Lazy<glm::quat> principle_axes_;
	mutable Lazy<std::vector<std::uint32_t>> triangle_indices_;
	mutable Lazy<std::vector<Edge>> edges_;
	mutable Lazy<size_t> skipped_faces_;
	mutable Lazy<half_edge::Topology> topology_;

public:
	Mesh() = default;

	Mesh(std::vector<Position> positions, std::vector<Normal> normals, FaceList faces)
		: positions_(std::move(positions))
		, normals_(std::move(normals))
		, faces_(std::move(faces))
	{
	}

	template <typename... Args>
	static std::optional<Mesh> fromObj(Args&&... args)
	{
		std::optional<Mesh> result;
		auto isOk = ObjFile::read(
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
		if (isOk)
		{
			return result;
		}
		return std::nullopt;
	}

	[[nodiscard]] Mesh withPositions(std::vector<Position> p) const { return Mesh{std::move(p), normals_, faces_}; }
	[[nodiscard]] Mesh withNormals(std::vector<Normal> n) const    { return Mesh{positions_, std::move(n), faces_}; }
	[[nodiscard]] Mesh withFaces(FaceList f) const                 { return Mesh{positions_, normals_, std::move(f)}; }

	[[nodiscard]]
	size_t numVertices() const
	{
		return positions_.size();
	}

	[[nodiscard]]
	size_t numFaces() const
	{
		return faces_.size();
	}

	/// @brief Check what type the faces are
	/// @return 0 = all faces tris, 1 = all faces quads, 2 = mixed/other
	[[nodiscard]]
	size_t faceType() const
	{
		auto tris = std::ranges::all_of(faces_.all(), [](const Face face) { return face.size() == 3; });
		if (tris)
		{
			return 0;
		}
		const auto quads = std::ranges::all_of(faces_.all(), [](const Face face) { return face.size() == 4; });
		return quads ? 1 : 2;
	}

	[[nodiscard]]
	size_t numEdges() const
	{
		const auto& data = edges_.getOrCreate([&] { return createEdges(); });
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
		const auto& edges = edges_.getOrCreate([&] { return createEdges(); });
		return edges;
	}

	[[nodiscard]]
	std::vector<Edge> boundaryEdges() const
	{
		const auto hes = topology().halfedges();

		std::vector<Edge> out;
		for (const auto& he : hes)
		{
			if (he.opposite.has_value())
			{
				continue;
			}

			const auto a = static_cast<std::uint32_t>(he.vertex);
			const auto b = static_cast<std::uint32_t>(hes[he.next].vertex);
			out.push_back(Edge{a, b});
		}
		return out;
	}

	[[nodiscard]] glm::quat principleAxes() const
	{
		return principle_axes_.getOrCreate([&] { return mesh::pca::compute(this->positions()); });
	}

	[[nodiscard]]
	std::span<const float> vertexPositionBuffer() const
	{
		static_assert(sizeof(glm::vec3) == sizeof(GLfloat) * 3, "Platform doesn't support this directly.");
		return std::span{reinterpret_cast<const float*>(positions_.data()), positions_.size() * 3};
	}

	[[nodiscard]]
	std::span<const float> vertexNormalBuffer() const
	{
		static_assert(sizeof(glm::vec3) == sizeof(GLfloat) * 3, "Platform doesn't support this directly.");
		return std::span{reinterpret_cast<const float*>(normals_.data()), normals_.size() * 3};
	}

	[[nodiscard]]
	Mesh translated(const glm::vec3& translation) const
	{
		return mesh::ops::transform::translate(*this, translation);
	}

	[[nodiscard]]
	Mesh filledHoles() const
	{
		return mesh::ops::repair::fillHoles(*this);
	}

	[[nodiscard]]
	Mesh rotated(const glm::quat& rotation) const
	{
		return mesh::ops::transform::rotate(*this, rotation);
	}

	[[nodiscard]]
	Mesh scaled(const glm::vec3& scale) const
	{
		return mesh::ops::transform::scale(*this, scale);
	}

	[[nodiscard]]
	Mesh withConsistentOrientations() const
	{
		return mesh::ops::repair::makeConsistentOrientations(*this);
	}

	[[nodiscard]] const Bounds& bounds() const
	{
		return bounds_.getOrCreate([&] { return createBounds(); });
	}

	[[nodiscard]] glm::vec3 baryCenter() const
	{
		return bary_center_.getOrCreate([&] { return mesh::ops::util::calcBaryCenter(positions_); });
	}

	[[nodiscard]] std::span<const std::uint32_t> triangleIndices() const
	{
		const auto& data = triangle_indices_.getOrCreate([&] { return createTriangleIndices(); });
		return std::span{data.begin(), data.size()};
	}

	[[nodiscard]] std::span<const std::uint32_t> edgeIndices() const
	{
		const auto& data = edges_.getOrCreate([&] { return createEdges(); });
		return std::span{reinterpret_cast<const uint32_t*>(data.data()), data.size() * 2};
	}

	[[nodiscard]] size_t numSkippedFaces() const
	{
		return skipped_faces_.getOrCreate([&] { return calcSkippedFaces(); });
	}

	[[nodiscard]] const half_edge::Topology& topology() const
	{
		return topology_.getOrCreate([&] { return half_edge::Topology::build(faces_, positions_.size()); });
	}

private:

	[[nodiscard]] Bounds createBounds() const
	{
		Bounds bounds{};
		for (const auto& position : positions_)
		{
			if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			{
				continue;
			}
			bounds.expand(position);
		}
		return bounds.valid() ? bounds : Bounds{glm::vec3{0.F}, glm::vec3{0.F}};
	}

	[[nodiscard]] std::vector<std::uint32_t> createTriangleIndices() const
	{
		std::vector<std::uint32_t> out;
		out.reserve(faces_.size() * 3);
		for (const auto [i, face] : faces_.all() | std::views::enumerate)
		{
			if (!FaceList::isValid(face, positions_.size()))
			{
				continue;
			}
			// Fan triangulation, only correct for convex polygons
			for (size_t faceIndex = 1; faceIndex + 1 < face.size(); ++faceIndex)
			{
				out.push_back(face[0]);
				out.push_back(face[faceIndex]);
				out.push_back(face[faceIndex + 1]);
			}
		}
		return out;
	}

	[[nodiscard]]
	std::vector<Edge> createEdges() const
	{
		std::vector<Edge> occurrences;
		occurrences.reserve(faces_.size() * 4);

		for (const auto& face : faces_.all())
		{
			if (!FaceList::isValid(face, positions_.size()))
			{
				continue;
			}

			for (std::size_t i = 0; i < face.size(); ++i)
			{
				const auto a = face[i];
				const auto b = face[(i + 1) % face.size()];
				if (a == b)
				{
					continue;
				}
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
			while (j < occurrences.size() && occurrences[j] == edge)
			{
				++j;
			}
			edges.push_back(edge);
			i = j;
		}
		return edges;
	}

	[[nodiscard]] size_t calcSkippedFaces() const
	{
		return static_cast<size_t>(std::ranges::count_if(faces_.all(), [this](const Face f) { return !FaceList::isValid(f, positions_.size()); }));
	}
};

#endif // MR_MESH_H
