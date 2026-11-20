//
// Created by dafitius on 09-10-2026.
//

#include "../mesh.hpp"
#include "ops.h"

#include <vector>

Mesh mesh::ops::transform::translate(const Mesh& mesh, const glm::dvec3& translation) {
	auto newPositions = std::vector<Mesh::Position>(mesh.positions().size());
	std::ranges::transform(mesh.positions(), newPositions.begin(), [translation](auto& pos) { return glm::vec3(glm::dvec3(pos) + translation); });
	return mesh.withPositions(newPositions);
}

Mesh mesh::ops::transform::rotate(const Mesh& mesh, const glm::quat& rotation) {
	auto newPositions = std::vector<Mesh::Position>(mesh.positions().size());
	std::ranges::transform(mesh.positions(), newPositions.begin(), [rotation](const auto& pos) { return rotation * pos; });

	auto newNormals = std::vector<Mesh::Normal>(mesh.normals().size());
	std::ranges::transform(mesh.normals(), newNormals.begin(), [rotation](const auto& normal) { return rotation * normal; });

	return mesh.withPositions(newPositions).withNormals(newNormals);
}

Mesh mesh::ops::transform::scale(const Mesh& mesh, const glm::vec3& scale) {
	auto newPositions = std::vector<Mesh::Position>(mesh.positions().size());
	std::ranges::transform(mesh.positions(), newPositions.begin(), [scale](auto& pos) { return pos * scale; });

	return mesh.withPositions(newPositions);
}