//
// Created by DanioHans on 05-11-2026.
//
#include "feature_vector.hpp"

#include "../geometry/features/features.h"
#include "../geometry/mesh.hpp"
#include "../stats/distributions.hpp"

namespace querying
{
	// FNV-1a hash function : https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	std::uint32_t shapeSeed(const std::string_view fileName, const std::uint32_t baseSeed)
	{
		std::uint32_t hash = 2166136261U;
		for (const char c : fileName)
		{
			hash ^= static_cast<std::uint8_t>(c);
			hash *= 16777619U;
		}
		return hash ^ (baseSeed * 2654435761U);
	}

	FeatureVector extractFeatures(const Mesh& mesh, const std::uint32_t seed)
	{
		FeatureVector features{
			.global =
				{mesh.volume(), mesh.surfaceArea(), mesh.compactness(), mesh.rectangularity(), mesh.diameter(), mesh.convexity(),
				 mesh.eccentricity()},
			.histograms = {},
		};

		mesh::features::property_descriptors::seedSampling(seed);
		for (const auto& distribution : stats::distributions())
		{
			const auto samples = distribution.sample(mesh);
			features.histograms.push_back(stats::binHistogram(samples, distribution.range, histogramBinCount));
		}
		return features;
	}
} // namespace querying
