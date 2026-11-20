//
// Created by DanioHans on 05-11-2026.
//
#include "feature_tree.hpp"

#include <nanoflann.hpp>

#include <cmath>

namespace querying
{
	namespace
	{
		struct PointCloud {
			std::size_t dimensions = 0;
			std::vector<double> values;

			[[nodiscard]] std::size_t kdtree_get_point_count() const {
				return dimensions == 0 ? 0 : values.size() / dimensions;
			}

			[[nodiscard]] double kdtree_get_pt(const std::size_t index, const std::size_t dimension) const {
				return values[index * dimensions + dimension];
			}

			template <class BoundingBox>
			bool kdtree_get_bbox(BoundingBox& /*box*/) const {
				return false;
			}
		};

		using L1Index = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L1_Adaptor<double, PointCloud>, PointCloud, -1, std::size_t>;
		using L2Index = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Adaptor<double, PointCloud>, PointCloud, -1, std::size_t>;

		constexpr std::size_t cumulativeBinCount = histogramBinCount - 1;
	} // namespace

	struct FeatureTree::Impl {
		std::vector<double> weights;
		TreeMetric metric;
		bool withHistograms;
		PointCloud points;
		std::unique_ptr<L1Index> l1;
		std::unique_ptr<L2Index> l2;

		[[nodiscard]] std::vector<double> point(const FeatureVector& features) const {
			const std::size_t histogramCount = withHistograms ? features.histograms.size() : 0;

			double globalWeight = 0.0;
			for (std::size_t i = 0; i < globalFeatureCount; ++i)
				globalWeight += featureWeight(weights, i);

			double histogramWeight = 0.0;
			for (std::size_t h = 0; h < histogramCount; ++h)
				histogramWeight += featureWeight(weights, globalFeatureCount + h);

			const double activeGroupCount = static_cast<double>((globalWeight > 0.0) + (histogramWeight > 0.0));
			const double globalGroupShare = globalWeight > 0.0 ? 1.0 / (activeGroupCount * globalWeight) : 0.0;
			const double histogramGroupShare = histogramWeight > 0.0 ? 1.0 / (activeGroupCount * histogramWeight) : 0.0;
			const auto factor = [&](const double share) { return metric == TreeMetric::L1 ? share : std::sqrt(share); };

			std::vector<double> result;
			result.reserve(globalFeatureCount + histogramCount * cumulativeBinCount);

			for (std::size_t i = 0; i < globalFeatureCount; ++i) {
				const double share = globalGroupShare * featureWeight(weights, i);
				result.push_back(factor(share) * features.global[i]);
			}

			for (std::size_t h = 0; h < histogramCount; ++h) {
				const double share = histogramGroupShare * featureWeight(weights, globalFeatureCount + h) / cumulativeBinCount;
				double cumulative = 0.0;
				for (std::size_t bin = 0; bin < cumulativeBinCount; ++bin) {
					cumulative += features.histograms[h][bin];
					result.push_back(factor(share) * cumulative);
				}
			}
			return result;
		}
	};

	FeatureTree::FeatureTree(const std::vector<StoredShape>& shapes, std::vector<double> weights, const TreeMetric metric, const bool withHistograms)
		: impl(std::make_unique<Impl>()) {
		impl->weights = std::move(weights);
		impl->metric = metric;
		impl->withHistograms = withHistograms;
		for (const auto& shape : shapes) {
			const auto point = impl->point(shape.features);
			impl->points.dimensions = point.size();
			impl->points.values.insert(impl->points.values.end(), point.begin(), point.end());
		}

		const auto dimensions = static_cast<int>(impl->points.dimensions);
		if (metric == TreeMetric::L1)
			impl->l1 = std::make_unique<L1Index>(dimensions, impl->points);
		else
			impl->l2 = std::make_unique<L2Index>(dimensions, impl->points);
	}

	FeatureTree::~FeatureTree() = default;

	std::vector<std::pair<double, std::size_t>> FeatureTree::nearest(const FeatureVector& query, const std::size_t k) const {
		if (k == 0 || impl->points.kdtree_get_point_count() == 0)
			return {};

		const auto point = impl->point(query);
		std::vector<std::size_t> indices(k);
		std::vector<double> distances(k);
		const auto found = impl->l1 ? impl->l1->knnSearch(point.data(), k, indices.data(), distances.data())
									: impl->l2->knnSearch(point.data(), k, indices.data(), distances.data());

		std::vector<std::pair<double, std::size_t>> result;
		result.reserve(found);
		for (std::size_t i = 0; i < found; ++i) {
			result.emplace_back(impl->l2 ? std::sqrt(distances[i]) : distances[i], indices[i]);
		}
		return result;
	}

	const std::vector<double>& FeatureTree::weights() const {
		return impl->weights;
	}

	TreeMetric FeatureTree::metric() const {
		return impl->metric;
	}

	bool FeatureTree::withHistograms() const {
		return impl->withHistograms;
	}
} // namespace querying
