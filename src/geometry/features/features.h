#ifndef MULTIMEDIARETRIEVAL_FEATURES_H
#define MULTIMEDIARETRIEVAL_FEATURES_H

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class Mesh;

namespace mesh::features
{
	namespace global_descriptors
	{
		float surfaceArea(const Mesh& mesh);
		float volume(const Mesh& mesh);
		float compactness(const Mesh& mesh);
		float rectangularity(const Mesh& mesh);
		float diameter(const Mesh& mesh);
		float convexity(const Mesh& mesh);
		float eccentricity(const Mesh& mesh);

		float signedVolumeOfTriangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
		float sphericity(const Mesh& mesh);
	} // namespace global_descriptors

	namespace distribution_descriptors
	{
		void seedSampling(std::uint32_t seed);

		std::vector<float> A3(const Mesh& mesh);
		std::vector<float> D1(const Mesh& mesh);
		std::vector<float> D2(const Mesh& mesh);
		std::vector<float> D3(const Mesh& mesh);
		std::vector<float> D4(const Mesh& mesh);
	} // namespace distribution_descriptors

	namespace local_curvature_descriptor
	{
		std::vector<float> curvatureIndex(const Mesh& mesh, size_t radius, size_t numBins);
		std::vector<float> minCurvatureFaceValues(const Mesh& mesh, size_t radius);
		std::vector<float> maxCurvatureFaceValues(const Mesh& mesh, size_t radius);
		std::vector<float> curvatureIndexFaceValues(const Mesh& mesh, size_t radius);
	} // namespace local_curvature_descriptor

} // namespace mesh::features

#endif // MULTIMEDIARETRIEVAL_FEATURES_H
