//
// Created by DanioHans on 15-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_METRICS_HPP
#define MULTIMEDIARETRIEVAL_METRICS_HPP

#include "../geometry/mesh.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace stats
{
	/// @brief A named number computed from a single mesh, one column in a statistics table.
	/// To add a statistic, add it to metrics() in metrics.cpp.
	struct Metric {
		std::string_view name;
		std::string_view description;
		double (*compute)(const Mesh&);
	};

	[[nodiscard]] std::span<const Metric> metrics();
	[[nodiscard]] std::optional<Metric> findMetric(std::string_view name);
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_METRICS_HPP
