//
// Created by DanioHans on 27-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP
#define MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP

#include "../geometry/mesh.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace stats
{
	struct BinRange
	{
		double min;
		double max;

		[[nodiscard]] double width() const
		{
			return max - min;
		}
	};

	/// @brief A shape property distribution (e.g. D2), sampled many times per mesh and reduced to a fixed length histogram.
	/// To add a distribution, add it to distributions() in distributions.cpp.
	struct Distribution
	{
		std::string_view name;
		std::string_view description;

		/// Fixed range of the histogram bins, the theoretical range for a shape that fits in a unit cube. Using the same range for
		/// every shape makes bin i mean the same for all of them, which is needed to compare histograms (e.g. with EMD).
		BinRange range;

		std::vector<float> (*sample)(const Mesh&);
	};

	[[nodiscard]] std::span<const Distribution> distributions();
	[[nodiscard]] std::optional<Distribution> findDistribution(std::string_view name);

	/// @brief Counts the samples in numBins equally sized bins over the range and normalizes the counts so they sum to 1.
	/// Samples outside the range are counted in the first or last bin, non-finite samples are ignored.
	/// @return numBins values, all 0 if there are no finite samples
	[[nodiscard]] std::vector<double> binHistogram(std::span<const float> samples, BinRange range, size_t numBins);
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP
