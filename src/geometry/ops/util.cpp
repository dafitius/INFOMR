//
// Created by dafitius on 09-10-2026.
//

#include <glm/vec3.hpp>
#include <qtransform.h>

namespace mesh::ops::util
{
	glm::dvec3 calcBaryCenter(const std::span<const glm::vec3> positions)
	{
		glm::dvec3 sum{0.0};

		for (const auto& position : positions)
		{
			sum += glm::dvec3(position);
		}

		return sum / static_cast<double>(positions.size());
	}
} // namespace mesh::ops::util