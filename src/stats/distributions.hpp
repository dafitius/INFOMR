//
// Created by DanioHans on 27-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP
#define MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP

#include "../geometry/mesh.hpp"
#include "../geometry/pipeline.hpp"
#include "../io/shape_db.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
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

	struct HistogramCsvResult
	{
		size_t shapesWritten = 0;
		size_t histogramCount = 0;
		size_t rowsWritten = 0;
		std::vector<std::filesystem::path> failed;
		bool written = false;
	};

	using HistogramProgressCallback = std::function<void(size_t done, size_t total)>;

	[[nodiscard]] std::span<const Distribution> distributions();
	[[nodiscard]] std::optional<Distribution> findDistribution(std::string_view name);

	/// @brief Counts the samples in numBins equally sized bins over the range and normalizes the counts so they sum to 1.
	/// Samples outside the range are counted in the first or last bin, non-finite samples are ignored.
	/// @return numBins values, all 0 if there are no finite samples
	[[nodiscard]] std::vector<double> binHistogram(std::span<const float> samples, BinRange range, size_t numBins);

	/// @brief Exports a binned A3/D1-D4 histogram for each shape to a long-form CSV.
	[[nodiscard]] HistogramCsvResult writePerShapeHistogramsCsv(
		const ShapeDb& database, std::span<const ShapeRef> shapes, const mesh::Pipeline& pipeline, const std::filesystem::path& path, size_t numBins,
		const std::atomic<bool>* cancel = nullptr, const HistogramProgressCallback& onProgress = {});

	/// @brief Earth Mover's Distance between two histograms over the same equally sized bins. the minimal amount of mass times the
	/// distance it has to move to turn one into the other. A shift of the same shape by one bin costs little, unlike bin by bin distances.
	/// Both histograms are normalized first, so it also accepts raw counts.
	/// @return [0, 1] regardless of the number of bins or their range: 0 for identical shapes, 1 for all mass at opposite ends.
	/// nullopt if the histograms differ in size or one of them is empty.
	[[nodiscard]] std::optional<double> earthMoversDistance(std::span<const double> first, std::span<const double> second);
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_DISTRIBUTIONS_HPP
