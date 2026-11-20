#ifndef QUERY_H
#define QUERY_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Mesh;

enum class DistanceMode {
	/// L2 on the global features and EMD on the histograms, averaged (brute force)
	Mix,
	/// L1 on the global features and the histogram bins (brute force)
	L1,
	/// L2 on the global features and the histogram bins (brute force)
	L2,
	/// L1 on the global features and the cumulative histogram bins, which is the EMD on the histograms (KD-tree)
	L1Emd,
	/// L1 on the global features only (KD-tree)
	GlobalsL1,
	/// L2 on the global features only (KD-tree)
	GlobalsL2
};

struct QueryResult {
	std::string name;
	std::string className;
	std::filesystem::path meshPath;
	double distance = 0.0;
	std::shared_ptr<const Mesh> normalizedMesh;

	QueryResult(const std::string name, const std::string className, const std::filesystem::path& meshPath, const double distance,
				std::shared_ptr<const Mesh> normalizedMesh = {})
		: name(name)
		, className(className)
		, meshPath(meshPath)
		, distance(distance)
		, normalizedMesh(std::move(normalizedMesh)) {}
};

class Query {
public:
	Query(std::filesystem::path queryMesh, std::vector<double> weights, DistanceMode mode = DistanceMode::Mix);

	~Query() = default;

	std::vector<QueryResult> search(std::size_t topK, bool loadMeshes = true);

private:
	std::filesystem::path queryMesh;
	std::vector<double> weights;
	DistanceMode mode;
};

#endif // QUERY_H
