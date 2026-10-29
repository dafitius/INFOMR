
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
	return {QueryResult{queryMesh.filename().string(), queryMesh, 0.0}};
}
