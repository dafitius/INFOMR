//
// Created by DanioHans on 05-11-2026.
//
#include "feature_vector.hpp"

#include "../geometry/features/features.h"
#include "../geometry/mesh.hpp"
#include "../stats/distributions.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iostream>
#include <ranges>

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

	double featureWeight(const std::vector<double>& weights, const std::size_t index)
	{
		if (index >= weights.size())
			return 1.0;

		return std::isfinite(weights[index]) ? std::max(0.0, weights[index]) : 0.0;
	}

	std::optional<std::vector<StoredShape>> loadFeatures(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file)
		{
			std::cerr << "Could not open " << path.string() << ", run generate_database first" << std::endl;
			return std::nullopt;
		}

		const std::size_t histogramCount = stats::distributions().size();
		const std::size_t columnCount = 2 + globalFeatureCount + histogramCount * histogramBinCount;

		std::string line;
		std::getline(file, line);

		std::vector<StoredShape> shapes;
		while (std::getline(file, line))
		{
			if (line.empty())
			{
				continue;
			}

			std::vector<std::string_view> columns;
			for (const auto column : std::views::split(std::string_view(line), ','))
			{
				columns.emplace_back(column.begin(), column.end());
			}
			if (columns.size() != columnCount)
			{
				std::cerr << path.string() << " has " << columns.size() << " columns instead of " << columnCount
						  << ", regenerate it with generate_database" << std::endl;
				return std::nullopt;
			}

			std::vector<double> values(columnCount - 2);
			for (std::size_t i = 0; i < values.size(); ++i)
			{
				const auto column = columns[i + 2];
				if (std::from_chars(column.data(), column.data() + column.size(), values[i]).ec != std::errc{})
				{
					std::cerr << "Invalid value '" << column << "' in " << path.string() << std::endl;
					return std::nullopt;
				}
			}

			StoredShape shape{.className = std::string(columns[0]), .fileName = std::string(columns[1]), .features = {}};
			std::ranges::copy_n(values.begin(), globalFeatureCount, shape.features.global.begin());
			for (std::size_t h = 0; h < histogramCount; ++h)
			{
				const auto first = values.begin() + static_cast<std::ptrdiff_t>(globalFeatureCount + h * histogramBinCount);
				shape.features.histograms.emplace_back(first, first + histogramBinCount);
			}
			shapes.push_back(std::move(shape));
		}
		return shapes;
	}
} // namespace querying
