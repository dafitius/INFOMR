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
