//
// Created by dafitius on 11-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_SUBMESH_HPP
#define MULTIMEDIARETRIEVAL_SUBMESH_HPP
#include "face_list.hpp"
#include "glm/vec3.hpp"
#include <span>

class Mesh;

class SubMesh
{
public:
	SubMesh(const std::span<const glm::vec3>& positions, const std::span<const glm::vec3>& normals, const FaceList& faceList)
		: positions_(positions)
		, normals_(normals)
		, face_list_(faceList)
	{
	}

	Mesh toMesh() const;

	[[nodiscard]] FaceList faceList() const
	{
		return face_list_;
	}

private:
	std::span<const glm::vec3> positions_;
	std::span<const glm::vec3> normals_;
	FaceList face_list_;
};


#endif // MULTIMEDIARETRIEVAL_SUBMESH_HPP
