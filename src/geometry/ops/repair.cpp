//
// Created by dafitius on 09-10-2026.
//

//
// Created by dafitius on 09-10-2026.
//

#include "../mesh.hpp"
#include "ops.h"

#include <vector>

namespace mesh::ops::repair
{
	Mesh fillHoles(const Mesh& mesh)
	{
		auto positions = std::vector<Mesh::Position>(mesh.positions().size());
		positions.assign(mesh.positions().begin(), mesh.positions().end());

		auto faces = mesh.faces();
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
				faces.push_back(window[0], window[1], centerIndex);
			}
		}

		return mesh.withPositions(positions).withFaces(faces);
	}

	Mesh makeConsistentOrientations(const Mesh& mesh)
	{
		auto newFaces = mesh.faces();

		const auto& topo = mesh.topology();
		const auto hes = topo.halfedges();

		std::vector visitedFaces(newFaces.size(), false);
		std::vector<half_edge::FaceRef> stack;
		stack.reserve(newFaces.size());

		auto sameDirectionInFace = [&](const half_edge::FaceRef f, const half_edge::EdgeKey edge) -> std::optional<bool>
		{
			const auto face = newFaces[f];

			for (std::size_t i = 0; i < face.size(); ++i)
			{
				const auto vertex0 = face[i];
				const auto vertex1 = face[(i + 1) % face.size()];

				if (vertex0 == edge.a && vertex1 == edge.b)
				{
					return true;
				}
				if (vertex0 == edge.b && vertex1 == edge.a)
				{
					return false;
				}
			}
			return std::nullopt;
		};

		constexpr half_edge::FaceRef seed = 0;
		visitedFaces[seed] = true;
		stack.push_back(seed);

		while (!stack.empty())
		{
			const auto f = stack.back();
			stack.pop_back();

			const auto fh = topo.getHalfedgeFromFace(f);
			if (!fh)
			{
				continue;
			}

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
							newFaces.reverseFace(other);
						}

						visitedFaces[other] = true;
						stack.push_back(other);
					}
				}

				h = he.next;
				if (h == start)
				{
					break;
				}
			}
		}
		return mesh.withFaces(newFaces);
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

		if (mesh.faceType() != Mesh::FaceType::TRI)
		{
			std::cerr << "Loop subdivision needs a triangle mesh" << "\n";
			return Mesh{};
		}

		if (mesh.topology().isClosed())
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
			const glm::vec3 b = pos[hes[he.next].vertex];

			glm::vec3 p{};
			if (he.opposite)
			{
				const glm::vec3 c = pos[hes[he.prev].vertex];
				const glm::vec3 d = pos[hes[hes[*he.opposite].prev].vertex];
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
			const auto ring = topo.vertexRing(v);
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

			const auto v0 = static_cast<std::uint32_t>(hes[h0].vertex);
			const auto v1 = static_cast<std::uint32_t>(hes[h1].vertex);
			const auto v2 = static_cast<std::uint32_t>(hes[h2].vertex);

			const auto e0 = static_cast<std::uint32_t>(edgeVert[h0]); // v0->v1
			const auto e1 = static_cast<std::uint32_t>(edgeVert[h1]); // v1->v2
			const auto e2 = static_cast<std::uint32_t>(edgeVert[h2]); // v2->v0

			outFaces.push_back({v0, e0, e2});
			outFaces.push_back({v1, e1, e0});
			outFaces.push_back({v2, e2, e1});
			outFaces.push_back({e0, e1, e2});
		}

		// Rebuild face list.
		FaceList faces;
		faces.reserve(outFaces.size(), outFaces.size() * 3);

		for (const auto& tri : outFaces)
		{
			faces.push_back(tri[0], tri[1], tri[2]);
		}

		return mesh.withPositions(std::move(newPos)).withFaces(std::move(faces));
	}

} // namespace mesh::ops::repair
