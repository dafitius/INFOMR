//
// Created by dafitius on 11-10-2026.
//

#include "submesh.h"
#include "mesh.hpp"

Mesh SubMesh::toMesh() const {
	FaceList newFaceList;
	newFaceList.reserve(face_list_.size(), face_list_.size() * 3); // When reserving we assume the submesh is triangles.
	std::vector<Mesh::Position> positions{};
	positions.reserve(face_list_.size() * 3);
	std::vector<Mesh::Normal> normals{};
	normals.reserve(face_list_.size() * 3);

	std::unordered_map<size_t, size_t> visited;

	for (const auto& face : face_list_.all()) {
		for (const auto& idx : face) {
			if (!visited.contains(idx)) {
				auto newIndex = visited.size();
				newFaceList.push_index(newIndex);
				visited.insert_or_assign(idx, newIndex);
				if (const auto* pos = idx < positions_.size() ? &positions_[idx] : nullptr) {
					positions.push_back(*pos);
				}
				if (const auto* norm = idx < normals_.size() ? &normals_[idx] : nullptr) {
					normals.push_back(*norm);
				}
			} else {
				newFaceList.push_index(visited[idx]);
			}
		}
		newFaceList.end_face();
	}
	return Mesh{
		positions,
		normals,
		newFaceList,
	};
}
