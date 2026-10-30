//
// Created by DanioHans on 15-10-2026.
//

#include "metrics.hpp"

#include "../geometry/face_list.hpp"
#include "../geometry/features/features.h"
#include "../geometry/mesh.hpp"
#include "../geometry/topology/halfedge.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

namespace stats
{
	namespace
	{
		/// @brief Applies fn(a, b, c) to every triangle of the fan triangulation of each valid face
		template <typename Fn>
		void forEachTriangle(const Mesh& mesh, Fn&& fn)
		{
			const auto positions = mesh.positions();
			for (const auto face : mesh.faces().list().all())
			{
				if (!FaceList::isValid(face, positions.size()))
				{
					continue;
				}
				for (size_t i = 1; i + 1 < face.size(); ++i)
				{
					fn(positions[face[0]], positions[face[i]], positions[face[i + 1]]);
				}
			}
		}

		/// @brief Counts faces with (near) zero area, e.g. collinear vertices or a vertex used twice
		double countDegenerateFaces(const Mesh& mesh)
		{
			const auto positions = mesh.positions();
			const auto diagonal = 2.0F * mesh.bounds().radius();
			const auto threshold = 1e-10F * diagonal * diagonal;

			return static_cast<double>(static_cast<std::int64_t>(std::ranges::count_if(
				mesh.faces().list().all(),
				[&](const Mesh::Face face)
				{
					if (!FaceList::isValid(face, positions.size()))
					{
						return false;
					}

					glm::vec3 areaVector{0.0F};
					for (size_t i = 1; i + 1 < face.size(); ++i)
					{
						areaVector += glm::cross(positions[face[i]] - positions[face[0]], positions[face[i + 1]] - positions[face[0]]);
					}
					return 0.5F * glm::length(areaVector) <= threshold;
				})));
		}

		/// @brief Number of edges shared by more than two faces
		double countNonManifoldEdges(const Mesh& mesh)
		{
			std::vector<std::pair<size_t, size_t>> edges;
			edges.reserve(mesh.faces().list().indices().size());
			for (const auto face : mesh.faces().list().all())
			{
				if (!FaceList::isValid(face, mesh.numVertices()))
				{
					continue;
				}
				for (size_t i = 0; i < face.size(); ++i)
				{
					const auto from = face[i];
					const auto target = face[(i + 1) % face.size()];
					if (from != target)
					{
						edges.emplace_back(std::min(from, target), std::max(from, target));
					}
				}
			}
			std::ranges::sort(edges);

			return static_cast<double>(std::ranges::count_if(
				edges | std::views::chunk_by(std::equal_to{}), [](const auto& sameEdge) { return std::ranges::distance(sameEdge) > 2; }));
		}

		/// @brief Number of edges whose two faces walk the edge in the same direction, so one of them faces the wrong way
		double countInconsistentEdges(const Mesh& mesh)
		{
			const auto halfedges = mesh.topology().halfedges();
			const auto count = std::ranges::count_if(
				halfedges, [&](const half_edge::Halfedge& halfedge)
				{ return halfedge.opposite.has_value() && halfedges[*halfedge.opposite].vertex == halfedge.vertex; });

			// Both halfedges of an inconsistent edge are counted
			return static_cast<double>(count) / 2.0;
		}

		/// @return For every vertex whether a valid face uses it
		std::vector<bool> referencedVertices(const Mesh& mesh)
		{
			std::vector referenced(mesh.numVertices(), false);
			for (const auto face : mesh.faces().list().all())
			{
				if (FaceList::isValid(face, mesh.numVertices()))
				{
					for (const auto vertex : face)
					{
						referenced[vertex] = true;
					}
				}
			}
			return referenced;
		}

		double countUnreferencedVertices(const Mesh& mesh)
		{
			return static_cast<double>(std::ranges::count(referencedVertices(mesh), false));
		}

		/// @brief Number of connected pieces, vertices that are not used by any face are not counted
		double countComponents(const Mesh& mesh)
		{
			std::vector<size_t> parent(mesh.numVertices());
			std::iota(parent.begin(), parent.end(), size_t{0});
			auto root = [&parent](size_t vertex)
			{
				while (parent[vertex] != vertex)
				{
					parent[vertex] = parent[parent[vertex]];
					vertex = parent[vertex];
				}
				return vertex;
			};

			for (const auto face : mesh.faces().list().all())
			{
				if (!FaceList::isValid(face, mesh.numVertices()))
				{
					continue;
				}
				for (const auto vertex : face)
				{
					parent[root(vertex)] = root(face[0]);
				}
			}

			const auto referenced = referencedVertices(mesh);
			size_t count = 0;
			for (size_t vertex = 0; vertex < parent.size(); ++vertex)
			{
				if (referenced[vertex] && root(vertex) == vertex)
				{
					++count;
				}
			}
			return static_cast<double>(count);
		}

		glm::vec3 extent(const Mesh& mesh)
		{
			return mesh.bounds().max - mesh.bounds().min;
		}
	} // namespace

	std::span<const Metric> metrics()
	{
		static constexpr std::array all{
			Metric{"vertices", "Number of vertices", [](const Mesh& m) { return static_cast<double>(m.numVertices()); }},
			Metric{"faces", "Number of faces", [](const Mesh& m) { return static_cast<double>(m.numFaces()); }},
			Metric{
				"non_triangle_faces", "Number of faces with more than three vertices",
				[](const Mesh& m)
				{
					return static_cast<double>(
						static_cast<std::int64_t>(std::ranges::count_if(m.faces().list().all(), [](const Mesh::Face f) { return f.size() != 3; })));
				}},
			Metric{"min_x", "Bounding box minimum x", [](const Mesh& m) { return static_cast<double>(m.bounds().min.x); }},
			Metric{"min_y", "Bounding box minimum y", [](const Mesh& m) { return static_cast<double>(m.bounds().min.y); }},
			Metric{"min_z", "Bounding box minimum z", [](const Mesh& m) { return static_cast<double>(m.bounds().min.z); }},
			Metric{"max_x", "Bounding box maximum x", [](const Mesh& m) { return static_cast<double>(m.bounds().max.x); }},
			Metric{"max_y", "Bounding box maximum y", [](const Mesh& m) { return static_cast<double>(m.bounds().max.y); }},
			Metric{"max_z", "Bounding box maximum z", [](const Mesh& m) { return static_cast<double>(m.bounds().max.z); }},
			Metric{"extent_x", "Bounding box size along x", [](const Mesh& m) { return static_cast<double>(extent(m).x); }},
			Metric{"extent_y", "Bounding box size along y", [](const Mesh& m) { return static_cast<double>(extent(m).y); }},
			Metric{"extent_z", "Bounding box size along z", [](const Mesh& m) { return static_cast<double>(extent(m).z); }},
			Metric{"bounding_radius", "Bounding box radius", [](const Mesh& m) { return static_cast<double>(m.bounds().radius()); }},
			Metric{
				"max_extent", "Largest bounding box side, 1 after scaling to the unit cube",
				[](const Mesh& m)
				{
					const auto size = extent(m);
					return static_cast<double>(std::max({size.x, size.y, size.z}));
				}},
			Metric{"bary_x", "Barycenter x", [](const Mesh& m) { return static_cast<double>(m.baryCenter().x); }},
			Metric{"bary_y", "Barycenter y", [](const Mesh& m) { return static_cast<double>(m.baryCenter().y); }},
			Metric{"bary_z", "Barycenter z", [](const Mesh& m) { return static_cast<double>(m.baryCenter().z); }},
			Metric{
				"bary_distance", "Distance from the barycenter to the origin, 0 after centering",
				[](const Mesh& m) { return static_cast<double>(glm::length(m.baryCenter())); }},
			Metric{
				"boundary_edges", "Edges used by only one face",
				[](const Mesh& m) { return static_cast<double>(std::ranges::distance(m.topology().boundaryHalfedges())); }},
			Metric{
				"holes", "Number of closed loops of boundary edges",
				[](const Mesh& m) { return static_cast<double>(m.topology().boundaryLoops().size()); }},
			Metric{"inconsistent_edges", "Edges where the two faces disagree on orientation (one normal flipped)", countInconsistentEdges},
			Metric{"non_manifold_edges", "Edges used by more than two faces", countNonManifoldEdges},
			Metric{"components", "Number of connected pieces", countComponents},
			Metric{"degenerate_faces", "Faces with (near) zero area", countDegenerateFaces},
			Metric{"unreferenced_vertices", "Vertices not used by any face", countUnreferencedVertices},
			Metric{
				"skipped_faces", "Faces with too few or out of range vertex indices",
				[](const Mesh& m) { return static_cast<double>(m.numSkippedFaces()); }},

			// Global descriptors
			Metric{
				"surface_area", "Surface area of the object",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::surfaceArea(m)); }},
			Metric{
				"volume", "Volume of the object", [](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::volume(m)); }},
			Metric{
				"compactness", "Compactness of the object, measured through sphericity",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::compactness(m)); }},
			Metric{
				"rectangularity", "3D rectangularity of the object",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::rectangularity(m)); }},
			Metric{
				"diameter", "Diameter of the object",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::diameter(m)); }},
			Metric{
				"convexity", "Convexity of the object",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::convexity(m)); }},
			Metric{
				"eccentricity", "Eccentricity of the object",
				[](const Mesh& m) { return static_cast<double>(mesh::features::global_descriptors::eccentricity(m)); }},
		};
		return all;
	}

	std::optional<Metric> findMetric(const std::string_view name)
	{
		const auto all = metrics();
		const auto found = std::ranges::find(all, name, &Metric::name);
		return found != all.end() ? std::make_optional(*found) : std::nullopt;
	}
} // namespace stats
