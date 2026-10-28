//
// Created by DanioHans on 15-10-2026.
//

#include "pipeline.hpp"

#include "../io/util.hpp"
#include "mesh.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/vector_float3.hpp>

namespace mesh
{
	std::span<const Operation> operations()
	{
		static constexpr std::array all{
			Operation{
				"orient_faces", "Flip faces so neighbouring faces have a consistent orientation",
				[](const Mesh& mesh) { return mesh.withConsistentOrientations(); }},
			Operation{"triangulate", "Triangulate the mesh", [](const Mesh& mesh) { return mesh.triangulated(); }},
			Operation{"remove_non-manifold", "Removes all faces that are non-manifold", [](const Mesh& mesh) { return mesh.removeNonManifold(); }},
			Operation{"fill_holes", "Close every boundary loop with a triangle fan", [](const Mesh& mesh) { return mesh.filledHoles(); }},
			Operation{"subdivide", "Subdivide the mesh", [](const Mesh& mesh) { return mesh.subdivided(); }},
			Operation{
				"center", "Translate the shape so its barycenter is at the origin",
				[](const Mesh& mesh) { return mesh.translated(-mesh.baryCenter()); }},
			Operation{
				"align_axes", "Rotate the shape so its principal axes align with the world axes (center first)",
				[](const Mesh& mesh) { return mesh.rotated(glm::inverse(mesh.principleAxes())); }},
			Operation{
				"flip object", "Flip the mesh based on its moment",
				[](const Mesh& mesh) { return mesh.moment().x < 0 ? mesh.rotated(glm::quat{0, 0, 0, 1}) : Mesh::copy(mesh); }},
			Operation{
				"scale_to_unit", "Scale the shape uniformly so its largest bounding box side is 1",
				[](const Mesh& mesh)
				{
					const auto size = mesh.bounds().max - mesh.bounds().min;
					const auto largest = std::max({size.x, size.y, size.z});
					return largest > 0.0F ? mesh.scaled(glm::vec3{1.0F / largest}) : Mesh::copy(mesh);
				}},
		};
		return all;
	}

	std::optional<Operation> findOperation(const std::string_view name)
	{
		const auto all = operations();
		const auto found = std::ranges::find(all, name, &Operation::name);
		return found != all.end() ? std::make_optional(*found) : std::nullopt;
	}

	std::optional<Pipeline> Pipeline::parse(const std::string_view text)
	{
		Pipeline pipeline;
		const auto trimmed = io_utils::trim(text);
		if (trimmed.empty() || trimmed == emptyLabel)
		{
			return pipeline;
		}

		for (const auto part : io_utils::split(trimmed, '>'))
		{
			const auto operation = findOperation(io_utils::trim(part));
			if (!operation)
			{
				return std::nullopt;
			}
			pipeline.add(*operation);
		}
		return pipeline;
	}

	std::string Pipeline::label() const
	{
		if (steps_.empty())
		{
			return std::string(emptyLabel);
		}

		std::string result;
		for (const auto& step : steps_)
		{
			if (!result.empty())
			{
				result += separator;
			}
			result += step.name;
		}
		return result;
	}

	Mesh Pipeline::apply(const Mesh& mesh) const
	{
		auto current = std::make_optional(Mesh::copy(mesh));
		for (const auto& step : steps_)
		{
			// The new mesh is built before the old one is destroyed, so it can be read from
			current.emplace(step.apply(*current));
		}
		return std::move(*current);
	}
} // namespace mesh
