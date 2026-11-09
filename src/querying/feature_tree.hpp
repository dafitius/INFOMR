//
// Created by DanioHans on 05-11-2026.
//

#ifndef MULTIMEDIARETRIEVAL_FEATURE_TREE_HPP
#define MULTIMEDIARETRIEVAL_FEATURE_TREE_HPP

#include "feature_vector.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace querying
{
	enum class TreeMetric
	{
		L1,
		L2
	};

	class FeatureTree
	{
	public:
		FeatureTree(const std::vector<StoredShape>& shapes, std::vector<double> weights, TreeMetric metric, bool withHistograms);
		~FeatureTree();

		FeatureTree(const FeatureTree&) = delete;
		FeatureTree& operator=(const FeatureTree&) = delete;

		[[nodiscard]] std::vector<std::pair<double, std::size_t>> nearest(const FeatureVector& query, std::size_t k) const;

		[[nodiscard]] const std::vector<double>& weights() const;
		[[nodiscard]] TreeMetric metric() const;
		[[nodiscard]] bool withHistograms() const;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
} // namespace querying

#endif // MULTIMEDIARETRIEVAL_FEATURE_TREE_HPP
