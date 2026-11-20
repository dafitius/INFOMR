//
// Created by dafitius on 11-10-2026.
//

#include "../mesh.hpp"
#include "../submesh.h"
#include "libqhullcpp/QhullError.h"
#include "libqhullcpp/QhullFacetList.h"
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertexSet.h>

namespace mesh::ops::derive
{
	std::optional<SubMesh> makeConvexHull(const Mesh& mesh) {
		const auto positions = mesh.positions();
		const auto normals = mesh.normals();

		FaceList hullFaces;

		std::vector<double> coords;
		std::vector<std::uint32_t> parentIndex;
		coords.reserve(positions.size() * 3);
		parentIndex.reserve(positions.size());

		for (const auto& [idx, pos] : positions | std::views::enumerate) {
			coords.push_back(pos.x);
			coords.push_back(pos.y);
			coords.push_back(pos.z);
			parentIndex.push_back(idx);
		}

		try {
			orgQhull::Qhull qhull;
			qhull.runQhull("",
						   3,								   // 3 for 3D
						   static_cast<int>(positions.size()), // number of positions to compute on
						   coords.data(),					   // the position coords as 3 doubles
						   "Qt"								   // Qt command to signal that we want a triangle mesh as output
			);

			for (const auto& facet : qhull.facetList()) {
				if (!facet.isGood())
					continue;

				auto verts = facet.vertices().toStdVector();
				std::array<std::size_t, 3> ids{};
				for (std::size_t i = 0; i < 3; ++i) {
					ids[i] = static_cast<std::size_t>(verts[i].point().id());
				}

				const auto& p0 = positions[ids[0]];
				const auto& p1 = positions[ids[1]];
				const auto& p2 = positions[ids[2]];

				const glm::vec3 a = p1 - p0;
				const glm::vec3 b = p2 - p0;
				const glm::vec3 faceNormal = glm::cross(a, b);
				const glm::vec3 faceCenter = (p0 + p1 + p2) / 3.0F;

				if (glm::dot(faceNormal, faceCenter) < 0.0F)
					std::swap(ids[1], ids[2]);

				hullFaces.push_range(ids | std::views::transform([&](const auto idx) { return parentIndex[idx]; }));
			}
		}
		catch (const orgQhull::QhullError&) {
			return std::nullopt;
		}

		return SubMesh{positions, normals, hullFaces};
	}

	std::optional<SubMesh> makeTriangleMesh(const Mesh& mesh) {
		if (mesh.isTriangleMesh()) {
			return SubMesh{mesh.positions(), mesh.normals(), mesh.faceList()};
		}

		const auto positions = mesh.positions();
		const auto normals = mesh.normals();
		const auto& faces = mesh.faceList();

		FaceList triangleFaces;
		triangleFaces.reserve(faces.size() * 2, faces.size() * 3);

		for (const auto& face : faces.all()) {
			if (face.size() < 3) {
				continue;
			}

			if (face.size() == 3) {
				triangleFaces.push_back(face[0], face[1], face[2]);
				continue;
			}

			for (std::size_t i = 1; i + 1 < face.size(); ++i) {
				triangleFaces.push_back(face[0], face[i], face[i + 1]);
			}
		}

		return SubMesh{positions, normals, triangleFaces};
	}
} // namespace mesh::ops::derive