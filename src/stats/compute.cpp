//
// Created by DanioHans on 15-10-2026.
//

#include "compute.hpp"

#include "../geometry/mesh.hpp"
#include "../geometry/pipeline.hpp"
#include "../io/shape_db.hpp"
#include "metrics.hpp"
#include "stats_table.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <execution>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace stats
{
	std::vector<double> measure(const Mesh& mesh, const std::span<const Metric> selected)
	{
		std::vector<double> values;
		values.reserve(selected.size());
		std::ranges::transform(selected, std::back_inserter(values), [&mesh](const Metric& metric) { return metric.compute(mesh); });
		return values;
	}

	RunResult compute(
		const ShapeDb& database, const std::span<const ShapeRef> shapes, const mesh::Pipeline& pipeline, const std::span<const Metric> selected,
		const std::atomic<bool>* cancel, const ProgressCallback& onProgress)
	{
		struct Job
		{
			const ShapeRef* shape;
			std::optional<std::vector<double>> values;
			bool skipped = false;
		};

		std::vector<Job> jobs;
		jobs.reserve(shapes.size());
		for (const auto& shape : shapes)
		{
			jobs.push_back(Job{.shape = &shape, .values = std::nullopt});
		}

		std::atomic<size_t> done{0};
		std::for_each(
			std::execution::par, jobs.begin(), jobs.end(),
			[&](Job& job)
			{
				if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
				{
					job.skipped = true;
					return;
				}

				const auto path = database.resourcePath(job.shape->className, job.shape->fileName);
				if (path.extension() == ".obj")
				{
					try
					{
						if (const auto mesh = Mesh::fromObj(path))
						{
							job.values = measure(pipeline.apply(*mesh), selected);
						}
					}
					catch (const std::exception& error)
					{
						std::cerr << "Failed to process " << path.string() << ": " << error.what() << '\n';
					}
				}

				const auto count = done.fetch_add(1, std::memory_order_relaxed) + 1;
				if (onProgress)
				{
					onProgress(count, jobs.size());
				}
			});

		std::vector<std::string> columns;
		columns.reserve(selected.size());
		std::ranges::transform(selected, std::back_inserter(columns), [](const Metric& metric) { return std::string(metric.name); });

		RunResult result{.table = StatsTable(pipeline.label(), std::move(columns)), .failed = {}};
		for (auto& job : jobs)
		{
			if (job.values)
			{
				result.table.addRow(ShapeRow{.className = job.shape->className, .fileName = job.shape->fileName, .values = std::move(*job.values)});
			}
			else if (!job.skipped)
			{
				result.failed.push_back(database.resourcePath(job.shape->className, job.shape->fileName));
			}
		}
		result.table.sortRows();
		return result;
	}
} // namespace stats
