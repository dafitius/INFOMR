#include "../mesh.hpp"
#include "features.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace mesh::features::global_descriptors
{

	float surfaceArea(const Mesh& mesh)
	{
		auto faces = mesh.faces();
		auto vertices = mesh.positions();

		float sum = 0.0;

		for (size_t i = 0; i < faces.size(); i++)
		{
			auto face = faces[i];
			glm::vec3 corner = vertices[face[0]];
			glm::vec3 a = vertices[face[1]] - corner;
			glm::vec3 b = vertices[face[2]] - corner;

			sum += glm::length(glm::cross(a, b));
		}

		return sum / 2.0F;
	}

	float volume(const Mesh& mesh)
	{
		auto faces = mesh.faces();
		auto verts = mesh.positions();
		float sum = 0;

		for (size_t i = 0; i < faces.size(); i++)
		{
			auto face = faces[i];
			if (face.size() != 3)
				std::cerr << "Contains non-triangle faces" << std::endl;

			auto p1 = verts[face[0]];
			auto p2 = verts[face[1]];
			auto p3 = verts[face[2]];
			sum += signedVolumeOfTriangle(p1, p2, p3);
		}

		return std::abs(sum);
	}

	float sphericity(const Mesh& mesh)
	{
		auto vol = volume(mesh);
		auto area = surfaceArea(mesh);
		auto a = pow(std::numbers::pi, (float)1 / 3);
		auto b = pow(6 * vol, (float)2 / 3);
		auto c = (a * b) / area;
		return c;
	}

	float compactness(const Mesh& mesh)
	{
		return sphericity(mesh);
	}

	float rectangularity(const Mesh& mesh)
	{
		auto vol = volume(mesh);
		auto obbVolume = mesh.bounds().volume();
		return obbVolume == 0.0F ? 0.0F : vol / obbVolume;
	}

	float diameter(const Mesh& mesh)
	{
		auto convexHull = mesh.convexHull()->toMesh();
		float max = 0;
		auto points = convexHull.positions();
		for (size_t i = 0; i < points.size(); i++)
		{
			auto pointA = points[i];
			for (size_t j = points.size() - 1; i < j; j--)
			{
				auto pointB = points[j];
				auto dist = glm::length2(pointA - pointB);
				if (dist > max)
				{
					max = dist;
				}
			}
		}

		return std::sqrt(max);
	}

	float convexity(const Mesh& mesh)
	{
		auto vol = volume(mesh);
		auto convexHull = mesh.convexHull();
		if (!convexHull)
			return 0.0F;

		const auto hullMesh = convexHull->toMesh();
		const auto hullVolume = volume(hullMesh);

		std::cout << "Object vertices: " << mesh.positions().size() << ", hull vertices: " << hullMesh.positions().size()
				  << ", object volume: " << vol << ", hull volume: " << hullVolume << '\n';

		return hullVolume == 0.0F ? 0.0F : vol / hullVolume;
	}

	float eccentricity(const Mesh& mesh)
	{
		auto vals = mesh.eigenValues();
		return vals[0] / vals[2];
	}

	/// @brief Assumes tris and centered mesh
	/// @param face
	/// @return Volume of a single triangle
	float signedVolumeOfTriangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
	{
		auto v321 = p3.x * p2.y * p1.z;
		auto v231 = p2.x * p3.y * p1.z;
		auto v312 = p3.x * p1.y * p2.z;
		auto v132 = p1.x * p3.y * p2.z;
		auto v213 = p2.x * p1.y * p3.z;
		auto v123 = p1.x * p2.y * p3.z;
		return (1.0f / 6.0f) * (-v321 + v231 + v312 - v132 - v213 + v123);
	}
} // namespace mesh::features::global_descriptors