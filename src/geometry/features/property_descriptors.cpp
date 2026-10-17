#include "features.h"
#include <vector>

std::vector<float> mesh::features::property_descriptors::A3(const Mesh& mesh)
{
	return std::vector<float>();
}

/// @brief Assumes mesh is centered
/// @param mesh
/// @return
std::vector<float> mesh::features::property_descriptors::D1(const Mesh& mesh)
{
	auto verts = mesh.positions();
	std::vector<float> result = std::vector<float>();
	for (size_t i = 0; i < verts.size(); i++)
	{
		float distance = glm::length(verts[i]);
		result.emplace_back(distance);
	}

	return result;
}

std::vector<float> mesh::features::property_descriptors::D2(const Mesh& mesh)
{
	return std::vector<float>();
}

std::vector<float> mesh::features::property_descriptors::D3(const Mesh& mesh)
{
	return std::vector<float>();
}

std::vector<float> mesh::features::property_descriptors::D4(const Mesh& mesh)
{
	return std::vector<float>();
}
