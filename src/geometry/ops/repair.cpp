//
// Created by dafitius on 09-10-2026.
//

//
// Created by dafitius on 09-10-2026.
//

#include "../mesh.hpp"
#include "ops.h"

#include <vector>

Mesh mesh::ops::repair::fillHoles(const Mesh& mesh)
{
	auto positions = std::vector<Mesh::Position>(mesh.positions().size());
	positions.assign(mesh.positions().begin(), mesh.positions().end());

	auto faces = mesh.faces();
	auto& topo = mesh.topology();

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

Mesh mesh::ops::repair::makeConsistentOrientations(const Mesh& mesh)
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
				break;
		}
	}
	return mesh.withFaces(newFaces);
}

