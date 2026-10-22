//
// Created by dafitius on 11-10-2026.
//

#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullVertexSet.h>
#include "../submesh.h"
#include "../mesh.hpp"
#include "libqhullcpp/QhullError.h"
#include "libqhullcpp/QhullFacetList.h"

namespace mesh::ops::derive
{
	std::optional<SubMesh> makeConvexHull(const Mesh& mesh)
	{
		const auto positions = mesh.positions();
		const auto normals = mesh.normals();

		FaceList hullFaces;

		std::vector<double> coords;
		std::vector<std::uint32_t> parentIndex;
		coords.reserve(positions.size() * 3);
		parentIndex.reserve(positions.size());

		for (const auto& [idx, pos] : positions | std::views::enumerate)
		{
			coords.push_back(pos.x);
			coords.push_back(pos.y);
			coords.push_back(pos.z);
			parentIndex.push_back(idx);
		}

		try
		{
			orgQhull::Qhull qhull;
			qhull.runQhull(
				"",
				3,									//3 for 3D
				static_cast<int>(positions.size()), //number of positions to compute on
				coords.data(),						//the position coords as 3 doubles
				"Qt"								//Qt command to signal that we want a triangle mesh as output
				);

			for (const auto& facet : qhull.facetList())
			{
				if (!facet.isGood())
				{
					continue;
				}

				auto verts = facet.vertices().toStdVector();
				auto ids = verts
					| std::views::take(3)
					| std::views::transform([&](const auto& v)
					  { return parentIndex[static_cast<std::size_t>(v.point().id())]; });
				hullFaces.push_range(ids);
			}
		}
		catch (const orgQhull::QhullError&)
		{
			return std::nullopt;
		}

		return SubMesh{positions, normals, hullFaces};
	}

	std::optional<SubMesh> makeTriangleMesh(const Mesh& mesh)
	{
		if (mesh.isTriangleMesh())
		{
			return SubMesh{mesh.positions(), mesh.normals(), mesh.faceList()};
		}

		const auto positions = mesh.positions();
		const auto normals = mesh.normals();
		const auto& faces = mesh.faceList();

		FaceList triangleFaces;
		triangleFaces.reserve(faces.size() * 2, faces.size() * 3);

		for (const auto& face : faces.all())
		{
			if (face.size() < 3)
			{
				continue;
			}

			if (face.size() == 3)
			{
				triangleFaces.push_back(face[0], face[1], face[2]);
				continue;
			}

			for (std::size_t i = 1; i + 1 < face.size(); ++i)
			{
				triangleFaces.push_back(face[0], face[i], face[i + 1]);
			}
		}

		return SubMesh{positions, normals, triangleFaces};
	}
}