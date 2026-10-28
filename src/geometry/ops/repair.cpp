//
// Created by dafitius on 09-10-2026.
//

#include "../mesh.hpp"
#include "ops.h"

#include <unordered_set>
#include <vector>

namespace mesh::ops::repair
{
	Mesh fillHoles(const Mesh& mesh)
	{
		auto positions = std::vector<Mesh::Position>(mesh.positions().size());
		positions.assign(mesh.positions().begin(), mesh.positions().end());

		auto faces = mesh.faceList();
		const auto& topo = mesh.topology();

		for (const auto& loop : topo.boundaryLoops())
		{
			std::vector<Mesh::Position> boundaryPositions;
			boundaryPositions.reserve(loop.size());

			for (const auto vertex : loop)
			{
				boundaryPositions.push_back(mesh.positions()[vertex]);
			}

			const auto center = util::calcBaryCenter(boundaryPositions);

			const auto centerIndex = positions.size();
			positions.push_back(center);

			auto closedLoop = loop;
			closedLoop.push_back(loop.front());
			for (const auto window : closedLoop | std::views::slide(2))
			{
				faces.push_back(window[1], window[0], centerIndex);
			}
		}

		return mesh.withPositions(positions).withFaces(faces);
	}

	Mesh makeConsistentOrientations(const Mesh& mesh)
	{
		auto newFaces = FaceSet(mesh.faces().list());
		const auto incidence = newFaces.edgeIncidence();

		const auto numFaces = newFaces.list().size();

		// nullopt = unvisited, false = keep winding, true = flip winding
		std::vector<std::optional<bool>> flipState(numFaces, std::nullopt);
		std::vector<FaceList::Index> stack;
		stack.reserve(numFaces);

		struct Neighbor
		{
			FaceList::Index face;
			bool sameDirection;
		};

		std::vector<std::vector<Neighbor>> neighbors(numFaces);

		for (const auto& occurrences : incidence | std::views::values)
		{
			if (occurrences.size() != 2)
			{
				continue;
			}

			const auto& a = occurrences[0];
			const auto& b = occurrences[1];

			if (a.face == b.face)
			{
				continue;
			}

			const bool sameDirection = a.from == b.from && a.to == b.to;

			neighbors[a.face].push_back({b.face, sameDirection});
			neighbors[b.face].push_back({a.face, sameDirection});
		}

		for (FaceList::Index seed = 0; seed < numFaces; ++seed)
		{
			if (flipState[seed].has_value())
			{
				continue;
			}

			flipState[seed] = false;
			stack.push_back(seed);

			while (!stack.empty())
			{
				const FaceList::Index current = stack.back();
				stack.pop_back();

				for (const auto& [face, sameDirection] : neighbors[current])
				{
					const bool requiredState = flipState[current].value() ^ sameDirection;

					if (!flipState[face].has_value())
					{
						flipState[face] = requiredState;
						stack.push_back(face);
					}
					else if (flipState[face].value() != requiredState)
					{
						std::cerr << "Failed to make consistent orientation\n";
						return mesh;
					}
				}
			}
		}

		for (FaceList::Index f = 0; f < numFaces; ++f)
		{
			if (flipState[f].value())
			{
				newFaces.mutableList().reverseFace(f);
			}
		}

		return mesh.withFaces(newFaces);
	}

	using CutResult = std::pair<Mesh, std::vector<half_edge::EdgeKey>>;

	inline CutResult cutNonManifoldEdges(const Mesh& mesh)
	{
		auto positions = std::vector(mesh.positions().begin(), mesh.positions().end());

		auto faces = mesh.faceList();
		const auto& topo = mesh.topology();

		using VertexRef = FaceList::Index;
		using half_edge::FaceRef;

		std::vector<std::pair<half_edge::EdgeKey, std::vector<FaceRef>>> singularEdges;

		for (const auto& edge : topo.nonManifoldEdges())
		{
			singularEdges.emplace_back(edge, topo.getFaceswithEdge(edge));
		}

		std::vector<std::unordered_map<VertexRef, VertexRef>> replacements(faces.size());

		for (const VertexRef vertex : std::views::iota(std::size_t{0}, mesh.positions().size()))
		{
			const auto subsets = topo.vertexStarSubsets(vertex);

			if (subsets.empty())
			{
				continue;
			}

			for (const auto subsetIndex : std::views::iota(std::size_t{1}, subsets.size()))
			{
				const auto duplicate = positions.size();
				positions.push_back(mesh.positions()[vertex]);

				for (const FaceRef face : subsets[subsetIndex])
				{
					replacements[face][vertex] = duplicate;
				}
			}
		}

		for (const auto faceIndex : std::views::iota(std::size_t{0}, faces.size()))
		{
			for (const auto& [original, duplicate] : replacements[faceIndex])
			{
				faces.replace(faceIndex, original, duplicate);
			}
		}

		std::unordered_set<half_edge::EdgeKey, UndirectedEdgeKeyHash> cutEdgeSet;

		for (const auto& [originalEdge, incidentFaces] : singularEdges)
		{
			for (const FaceRef face : incidentFaces)
			{
				const auto remap = [&](const VertexRef vertex) -> VertexRef
				{
					const auto it = replacements[face].find(vertex);
					return it != replacements[face].end() ? it->second : vertex;
				};

				const half_edge::EdgeKey cutEdge{.a = remap(originalEdge.a), .b = remap(originalEdge.b)};

				cutEdgeSet.insert(cutEdge.canonize());
			}
		}

		std::vector const cutEdges(cutEdgeSet.begin(), cutEdgeSet.end());

		auto newMesh = mesh.withPositions(positions).withFaces(faces);
		return std::make_pair(newMesh, cutEdges);
	}

	Mesh makeManifold(const Mesh& mesh)
	{
		return cutNonManifoldEdges(mesh).first;
	}

	Mesh makeLoopSubdividedMesh(const Mesh& mesh)
	{
		// adapted from https://github.com/icemiliang/loop_subdivision/blob/6546efc2936a9e573f0c4133869680ef815dc14c/src/LOOP.cpp#L26
		auto loopAlpha = [](const std::size_t n)
		{
			if (n <= 3)
			{
				return 3.0F / 16.0F;
			}

			const float center = 0.375F + (0.25F * std::cos(2.0F * std::numbers::pi_v<float> / static_cast<float>(n)));
			return (0.625F - (center * center)) / static_cast<float>(n);
		};

		if (mesh.faceType() != FaceSet::FaceType::TRI)
		{
			std::cerr << "Loop subdivision needs a triangle mesh" << "\n";
			return Mesh{};
		}

		if (!mesh.topology().isClosed())
		{
			std::cerr << "Loop subdivision requires a closed mesh" << "\n";
			return Mesh{};
		}


		const auto& topo = mesh.topology();
		const auto& hes = topo.halfedges();
		const auto& pos = mesh.positions();
		const std::size_t numVertices = pos.size();

		std::vector edgeVert(hes.size(), std::numeric_limits<std::size_t>::max());
		std::vector newPos(pos.begin(), pos.end());

		for (auto h : std::views::iota(0u, hes.size()))
		{
			const auto& he = hes[h];
			if (he.opposite && *he.opposite < h)
			{
				continue;
			}

			const glm::vec3 a = pos[he.vertex];
			const glm::vec3 b = pos[hes[he.prev].vertex];

			glm::vec3 p{};
			if (he.opposite)
			{
				const glm::vec3 c = pos[hes[he.next].vertex];
				const glm::vec3 d = pos[hes[hes[*he.opposite].next].vertex];
				p = 0.375F * (a + b) + 0.125F * (c + d);
			}
			else
			{
				p = 0.5F * (a + b);
			}

			const std::size_t newIndex = newPos.size();
			edgeVert[h] = newIndex;
			if (he.opposite)
			{
				edgeVert[*he.opposite] = newIndex;
			}
			newPos.push_back(p);
		}

		// Reposition original vertices.
		for (std::size_t v = 0; v < numVertices; ++v)
		{
			const auto ring = topo.orderedVertexNeighbors(v);
			if (ring.empty())
			{
				continue;
			}

			const std::size_t n = ring.size();
			const float beta = loopAlpha(n);

			glm::vec3 sum{0.0F};
			for (const auto nb : ring)
			{
				sum += pos[nb];
			}

			newPos[v] = (1.0F - (static_cast<float>(n) * beta)) * pos[v] + beta * sum;
		}

		// Split the triangles in 4
		std::vector<std::array<std::uint32_t, 3>> outFaces;
		outFaces.reserve(mesh.numFaces() * 4);

		for (std::size_t f = 0; f < mesh.numFaces(); ++f)
		{
			const std::size_t h0 = 3 * f;
			const std::size_t h1 = h0 + 1;
			const std::size_t h2 = h0 + 2;

			const auto v0 = static_cast<std::uint32_t>(hes[h2].vertex); // A
			const auto v1 = static_cast<std::uint32_t>(hes[h0].vertex); // B
			const auto v2 = static_cast<std::uint32_t>(hes[h1].vertex); // C

			const auto e0 = static_cast<std::uint32_t>(edgeVert[h0]); // AB
			const auto e1 = static_cast<std::uint32_t>(edgeVert[h1]); // BC
			const auto e2 = static_cast<std::uint32_t>(edgeVert[h2]); // CA

			outFaces.push_back({v0, e0, e2});
			outFaces.push_back({v1, e1, e0});
			outFaces.push_back({v2, e2, e1});
			outFaces.push_back({e0, e1, e2});
		}

		FaceList faces;
		faces.reserve(outFaces.size(), outFaces.size() * 3);

		for (const auto& tri : outFaces)
		{
			faces.push_back(tri[0], tri[1], tri[2]);
		}

		return mesh.withPositions(std::move(newPos)).withFaces(std::move(faces));
	}

} // namespace mesh::ops::repair
