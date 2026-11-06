#ifndef QUERY_H
#define QUERY_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Mesh;

enum class DistanceMode
{
	Mix,
	L1,
	L2
};

struct QueryResult
{
	std::string name;
	std::string className;
	std::filesystem::path meshPath;
	double distance = 0.0;
	std::shared_ptr<const Mesh> normalizedMesh;

	QueryResult(
		const std::string name, const std::string className, const std::filesystem::path& meshPath, const double distance,
		std::shared_ptr<const Mesh> normalizedMesh = {})
		: name(name)
		, className(className)
		, meshPath(meshPath)
		, distance(distance)
		, normalizedMesh(std::move(normalizedMesh))
	{
	}
};

class Query
{
public:
	Query(std::filesystem::path queryMesh, std::vector<double> weights, DistanceMode mode = DistanceMode::Mix);

	~Query() = default;

	std::vector<QueryResult> search(std::size_t topK);

private:
	std::filesystem::path queryMesh;
	std::vector<double> weights;
	DistanceMode mode;
};

#endif // QUERY_H
