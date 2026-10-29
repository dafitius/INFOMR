#ifndef QUERY_H
#define QUERY_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct QueryResult
{
	std::string name;
	std::filesystem::path meshPath;
	double distance = 0.0;

	QueryResult(const std::string name, const std::filesystem::path& meshPath, const double distance)
		: name(name)
		, meshPath(meshPath)
		, distance(distance)
	{
	}
};

class Query
{
public:
	Query(std::filesystem::path queryMesh, std::vector<double> weights);

	~Query() = default;

	std::vector<QueryResult> search(std::size_t topK);

private:
	std::filesystem::path queryMesh;
	std::vector<double> weights;
};

#endif // QUERY_H
