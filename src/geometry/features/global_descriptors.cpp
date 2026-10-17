#include "../mesh.hpp"
#include "features.h"

#include <vector>

float mesh::features::global_descriptors::surfaceArea(const Mesh& mesh)
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

float mesh::features::global_descriptors::volume(const Mesh& mesh)
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

float mesh::features::global_descriptors::compactness(const Mesh& mesh)
{
	return size_t();
}

float mesh::features::global_descriptors::rectangularity(const Mesh& mesh)
{
	return size_t();
}

float mesh::features::global_descriptors::diameter(const Mesh& mesh)
{
	return size_t();
}

float mesh::features::global_descriptors::convexity(const Mesh& mesh)
{
	return size_t();
}

float mesh::features::global_descriptors::eccentricity(const Mesh& mesh)
{
	return size_t();
}

/// @brief Assumes tris and centered mesh
/// @param face
/// @return Volume of a single triangle
float mesh::features::global_descriptors::signedVolumeOfTriangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
{
	auto v321 = p3.x * p2.y * p1.z;
	auto v231 = p2.x * p3.y * p1.z;
	auto v312 = p3.x * p1.y * p2.z;
	auto v132 = p1.x * p3.y * p2.z;
	auto v213 = p2.x * p1.y * p3.z;
	auto v123 = p1.x * p2.y * p3.z;
	return (1.0f / 6.0f) * (-v321 + v231 + v312 - v132 - v213 + v123);
}