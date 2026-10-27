//
// Created by DanioHans on 27-10-2026.
//

#include "distributions.hpp"

#include "../geometry/features/features.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace stats
{
	namespace
	{
		constexpr double maxD1 = 0.8;
		constexpr double maxD2 = 1.2;
		constexpr double maxD3 = 0.65;
		constexpr double maxD4 = 0.35;
	} // namespace

	std::span<const Distribution> distributions()
	{
		namespace descriptors = mesh::features::property_descriptors;

		static constexpr std::array all{
			Distribution{"a3", "Angle between three random vertices", BinRange{.min = 0.0, .max = std::numbers::pi}, descriptors::A3},
			Distribution{"d1", "Distance between the barycenter and a random vertex", BinRange{.min = 0.0, .max = maxD1}, descriptors::D1},
			Distribution{"d2", "Distance between two random vertices", BinRange{.min = 0.0, .max = maxD2}, descriptors::D2},
			Distribution{
				"d3", "Square root of the area of the triangle between three random vertices", BinRange{.min = 0.0, .max = maxD3}, descriptors::D3},
			Distribution{
				"d4", "Cube root of the volume of the tetrahedron between four random vertices", BinRange{.min = 0.0, .max = maxD4}, descriptors::D4},
		};
		return all;
	}

	std::optional<Distribution> findDistribution(const std::string_view name)
	{
		const auto all = distributions();
		const auto found = std::ranges::find(all, name, &Distribution::name);
		return found != all.end() ? std::make_optional(*found) : std::nullopt;
	}

	std::vector<double> binHistogram(const std::span<const float> samples, const BinRange range, const size_t numBins)
	{
		std::vector histogram(numBins, 0.0);
		if (numBins == 0 || range.width() <= 0.0)
		{
			return histogram;
		}

		size_t total = 0;
		for (const auto sample : samples)
		{
			if (!std::isfinite(sample))
			{
				continue;
			}

			const auto position = std::clamp((static_cast<double>(sample) - range.min) / range.width(), 0.0, 1.0);
			const auto bin = std::min(static_cast<size_t>(position * static_cast<double>(numBins)), numBins - 1);
			histogram[bin] += 1.0;
			++total;
		}

		if (total > 0)
		{
			for (auto& value : histogram)
			{
				value /= static_cast<double>(total);
			}
		}
		return histogram;
	}
} // namespace stats
