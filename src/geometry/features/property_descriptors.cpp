#include "../mesh.hpp"
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

	/// Samples points as described by ACM Transactions on Graphics Volume 21, Issue 4: Shape distributions
	template <std::size_t N = MAX_SAMPLES>
	auto samplePoints(const Mesh& mesh)
	{
		const auto& cumulative = mesh.cumulativeFaceAreas();
		const auto& positions = mesh.positions();
		const auto& faces = mesh.faces().list();
		auto& rng = randomEngine();

		std::uniform_real_distribution areaPick(0.0, cumulative.back());
		std::uniform_real_distribution unit(0.0f, 1.0f);

		const auto triangleArea = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
		{ return 0.5 * glm::length(glm::cross(b - a, c - a)); };

		const auto sample = [&]
		{
			// "we select a face with probability proportional to its area."
			const double r = areaPick(rng);
			const auto faceIndex = std::min<std::size_t>(std::ranges::upper_bound(cumulative, r) - cumulative.begin(), cumulative.size() - 1);
			const auto& face = faces[faceIndex];

			double remaining = r - ((faceIndex != 0u) ? cumulative[faceIndex - 1] : 0.0);
			std::size_t k = 1;
			for (; k + 2 < face.size(); ++k)
			{
				const double area = triangleArea(positions[face[0]], positions[face[k]], positions[face[k + 1]]);
				if (remaining < area)
				{
					break;
				}
				remaining -= area;
			}

			// P = (1 - sqrt(r1)) A + sqrt(r1)(1 - r2) B + sqrt(r1) r2 C
			const float sqrtR1 = std::sqrt(unit(rng)); // √r1 sets the percentage from vertex A to the opposing edge, while ...
			const float r2 = unit(rng);				   // ... r2 represents the percentage along that edge.
			return (1.0f - sqrtR1) * positions[face[0]] + sqrtR1 * (1.0f - r2) * positions[face[k]] + sqrtR1 * r2 * positions[face[k + 1]];
		};

		if constexpr (N == 1)
		{
			return sample();
		}
		else
		{
			std::vector<glm::vec3> result(N);
			for (auto& p : result)
				p = sample();
			return result;
		}
	}

	template <std::size_t M = 1, std::size_t N = MAX_SAMPLES>
	auto samplePointSets(const Mesh& mesh)
	{
		if constexpr (N == 1)
		{
			return samplePoints<N>(mesh);
		}
		else
		{
			std::array<std::vector<glm::vec3>, M> result;
			for (auto& p : result)
				p = samplePoints<N>(mesh);
			return result;
		}
	}

} // namespace

namespace mesh::features::property_descriptors
{
	void seedSampling(const std::uint32_t seed)
	{
		randomEngine().seed(seed);
	}

	/// @brief A3: angle between 3 random vertices
	/// @param mesh
	/// @return
	std::vector<float> A3(const Mesh& mesh)
	{
		const auto verts = mesh.positions();
		if (verts.size() < 3)
			return {};

		const auto [a, b, c] = samplePointSets<3>(mesh);

		const std::size_t count = std::min({a.size(), b.size(), c.size()});

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			const glm::vec3 ab = b[i] - a[i];
			const glm::vec3 ac = c[i] - a[i];

			const float lenAB = glm::length(ab);
			const float lenAC = glm::length(ac);

			// Accidentally use same point
			if (lenAB == 0.0f || lenAC == 0.0f)
				continue;

			auto angle = std::acos(glm::dot(ab, ac) / (lenAB * lenAC));

			result.push_back(angle);
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

		const auto samples = samplePoints(mesh);

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

		const auto [a, b] = samplePointSets<2>(mesh);

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

		const auto [a, b, c] = samplePointSets<3>(mesh);

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

		const auto [a, b, c, d] = samplePointSets<4>(mesh);

		const std::size_t count = std::min({a.size(), b.size(), c.size(), d.size()});

		std::vector<float> result;
		result.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			// https://stackoverflow.com/questions/9866452/calculate-volume-of-any-tetrahedron-given-4-points
			const glm::vec3 ad = a[i] - d[i];
			const glm::vec3 bd = b[i] - d[i];
			const glm::vec3 cd = c[i] - d[i];
			const glm::vec3 cross = glm::cross(bd, cd);
			const float dot = glm::dot(ad, cross);
			float vol = std::abs(dot) / 6;

			result.push_back(std::cbrt(vol));
		}

		return result;
	}
} // namespace mesh::features::property_descriptors
