
#include "query.h"

Query::Query(std::filesystem::path queryMesh, std::vector<double> weights)
	: queryMesh(std::move(queryMesh))
	, weights(std::move(weights))
{
}

std::vector<QueryResult> Query::search(std::size_t topK)
{
	(void)topK;
	// TODO: Hardcoded return input model as only result

	// 1: Normalize uploaded model
	// 2: Calculate descriptors
	// 3: Create feature vector

	// 4: Find which classes need to be compared
	// 5: Calculate distances for every object in select classes

	// Framework for easy querying

	return {QueryResult{queryMesh.filename().string(), queryMesh, 0.0}};
}
