#ifndef MULTIMEDIARETRIEVAL_FEATURES_H
#define MULTIMEDIARETRIEVAL_FEATURES_H

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
	} // namespace global_descriptors

	namespace property_descriptors
	{
		std::vector<float> A3(const Mesh& mesh);
		std::vector<float> D1(const Mesh& mesh);
		std::vector<float> D2(const Mesh& mesh);
		std::vector<float> D3(const Mesh& mesh);
		std::vector<float> D4(const Mesh& mesh);
	} // namespace property_descriptors

} // namespace mesh::features

#endif // MULTIMEDIARETRIEVAL_FEATURES_H
