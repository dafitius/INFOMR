//
// Created by dafitius on 09-10-2026.
//

#include <glm/vec3.hpp>
#include <qtransform.h>

namespace mesh::ops::util
{
	glm::vec3 calcBaryCenter(std::span<const glm::vec3> points)
	{
		auto xIter = std::views::transform(points, [](auto& pos) { return pos.x; });
		auto yIter = std::views::transform(points, [](auto& pos) { return pos.y; });
		auto zIter = std::views::transform(points, [](auto& pos) { return pos.z; });

		double const xMean = std::accumulate(xIter.begin(), xIter.end(), 0.0) / static_cast<double>(points.size());
		double const yMean = std::accumulate(yIter.begin(), yIter.end(), 0.0) / static_cast<double>(points.size());
		double const zMean = std::accumulate(zIter.begin(), zIter.end(), 0.0) / static_cast<double>(points.size());
		return {xMean, yMean, zMean};
	};
} // namespace mesh::ops::util