//
// Created by dafitius on 09-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_OPS_H
#define MULTIMEDIARETRIEVAL_OPS_H
#include "glm/fwd.hpp"

class Mesh;
class SubMesh;

namespace mesh::ops
{
	namespace transform {
		Mesh translate(const Mesh& mesh, const glm::vec3& translation);
		Mesh rotate(const Mesh& mesh, const glm::quat& rotation);
		Mesh scale(const Mesh& mesh, const glm::vec3& scale);
	}

	namespace repair
	{
		Mesh fillHoles(const Mesh& mesh);
		Mesh makeConsistentOrientations(const Mesh& mesh);
		Mesh makeLoopSubdividedMesh(const Mesh& mesh);
		Mesh makeManifold(const Mesh& mesh);
	}

	namespace derive
	{
		std::optional<SubMesh> makeConvexHull(const Mesh& mesh);
		std::optional<SubMesh> makeTriangleMesh(const Mesh& mesh);
	}

	namespace util
	{
		glm::vec3 calcBaryCenter(std::span<const glm::vec3> points);
	}
}

#endif // MULTIMEDIARETRIEVAL_OPS_H
