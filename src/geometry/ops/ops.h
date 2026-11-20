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
	namespace transform
	{
		Mesh translate(const Mesh& mesh, const glm::dvec3& translation);
		Mesh rotate(const Mesh& mesh, const glm::quat& rotation);
		Mesh scale(const Mesh& mesh, const glm::vec3& scale);
	} // namespace transform

	namespace repair
	{
		Mesh fillHoles(const Mesh& mesh);
		Mesh makeConsistentOrientations(const Mesh& mesh);
		Mesh makeLoopSubdividedMesh(const Mesh& mesh, const bool smoothing);
		Mesh simplifyMesh(const Mesh& mesh, float reductionFactor);
		Mesh makeManifold(const Mesh& mesh);
	} // namespace repair

	namespace derive
	{
		std::optional<SubMesh> makeConvexHull(const Mesh& mesh);
		std::optional<SubMesh> makeTriangleMesh(const Mesh& mesh);
	} // namespace derive

	namespace util
	{
		glm::dvec3 calcBaryCenter(std::span<const glm::vec3> points);
	}
} // namespace mesh::ops

#endif // MULTIMEDIARETRIEVAL_OPS_H
