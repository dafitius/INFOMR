//
// Created by DanioHans on 15-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_COMPUTE_HPP
#define MULTIMEDIARETRIEVAL_COMPUTE_HPP

#include "../geometry/mesh.hpp"
#include "../geometry/pipeline.hpp"
#include "../io/shape_db.hpp"
#include "metrics.hpp"
#include "stats_table.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <vector>

namespace stats
{
	/// @brief Called with the number of processed shapes and the total, may be called from multiple threads at once
	using ProgressCallback = std::function<void(size_t done, size_t total)>;

	struct RunResult
	{
		StatsTable table;
		std::vector<std::filesystem::path> failed;
	};

	/// @return One value per metric, in the same order
	[[nodiscard]] std::vector<double> measure(const Mesh& mesh, std::span<const Metric> selected);

	/// @brief Loads the given shapes (in parallel), runs the pipeline on each of them and measures the result.
	/// The table is labelled with the pipeline and has one column per metric.
	/// @param cancel Optional flag, when set the remaining shapes are skipped (and not reported as failed)
	[[nodiscard]] RunResult compute(
		const ShapeDb& database, std::span<const ShapeRef> shapes, const mesh::Pipeline& pipeline, std::span<const Metric> selected = metrics(),
		const std::atomic<bool>* cancel = nullptr, const ProgressCallback& onProgress = {});
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_COMPUTE_HPP
