#include "features.h"
#include <algorithm>
#include <random>
#include <vector>

namespace
{
	constexpr std::size_t MAX_SAMPLES = 10'000;

	std::mt19937& randomEngine()
	{
		static thread_local std::mt19937 rng{std::random_device{}()};
		return rng;
	}

	template <typename Range>
	std::vector<glm::vec3> sampleVertices(const Range& vertices, std::size_t count = MAX_SAMPLES)
	{
		std::vector<glm::vec3> result;
		result.reserve(std::min(count, vertices.size()));

		std::sample(vertices.begin(), vertices.end(), std::back_inserter(result), count, randomEngine());
		std::shuffle(result.begin(), result.end(), randomEngine());

		return result;
	}
} // namespace

namespace mesh::features::property_descriptors
{
	/// @brief A3: angle between 3 random vertices
	/// @param mesh
	/// @return
	std::vector<float> A3(const Mesh& mesh)
	{
		const auto verts = mesh.positions();
		if (verts.size() < 3)
			return {};

		const auto a = sampleVertices(verts);
		const auto b = sampleVertices(verts);
		const auto c = sampleVertices(verts);

		const std::size_t count = std::min({a.size(), b.size(), c.size()});

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			// TODO
			result.push_back(-1);
		}

		return result;
	}

	/// @brief D1: distance between barycenter and random vertex. Assumes mesh is centered
	/// @param mesh
	/// @return
	std::vector<float> D1(const Mesh& mesh)
	{
		auto verts = mesh.positions();
		if (verts.empty())
			return {};

		const auto samples = sampleVertices(verts);

		std::vector<float> result;
		result.reserve(samples.size());

		for (const auto& vertex : samples)
			result.push_back(glm::length(vertex));

		return result;
	}

	/// @brief D2: distance between 2 random vertices
	/// @param mesh
	/// @return
	std::vector<float> D2(const Mesh& mesh)
	{
		const auto verts = mesh.positions();
		if (verts.size() < 2)
			return {};

		const auto a = sampleVertices(verts);
		const auto b = sampleVertices(verts);

		const std::size_t count = std::min(a.size(), b.size());

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
			result.push_back(glm::length(a[i] - b[i]));

		return result;
	}

	/// @brief D3: square root of area of triangle given by 3 random vertices
	/// @param mesh
	/// @return
	std::vector<float> D3(const Mesh& mesh)
	{
		const auto verts = mesh.positions();
		if (verts.size() < 3)
			return {};

		const auto a = sampleVertices(verts);
		const auto b = sampleVertices(verts);
		const auto c = sampleVertices(verts);

		const std::size_t count = std::min({a.size(), b.size(), c.size()});

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			const glm::vec3 ab = b[i] - a[i];
			const glm::vec3 ac = c[i] - a[i];

			const float area = 0.5f * glm::length(glm::cross(ab, ac));
			result.push_back(std::sqrt(area));
		}

		return result;
	}

	/// @brief D4: cube root of volume of tetrahedron formed by 4 random vertices
	/// @param mesh
	/// @return
	std::vector<float> D4(const Mesh& mesh)
	{
		const auto verts = mesh.positions();
		if (verts.size() < 4)
			return {};

		const auto a = sampleVertices(verts);
		const auto b = sampleVertices(verts);
		const auto c = sampleVertices(verts);
		const auto d = sampleVertices(verts);

		const std::size_t count = std::min({a.size(), b.size(), c.size(), d.size()});

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			// TODO
			result.push_back(-1);
		}

		return result;
	}
} // namespace mesh::features::property_descriptors
