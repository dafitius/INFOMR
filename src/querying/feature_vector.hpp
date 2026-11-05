//
// Created by DanioHans on 05-11-2026.
//

#ifndef MULTIMEDIARETRIEVAL_FEATURE_VECTOR_HPP
#define MULTIMEDIARETRIEVAL_FEATURE_VECTOR_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

class Mesh;

namespace querying
{
	constexpr std::size_t globalFeatureCount = 7;
	constexpr std::size_t histogramBinCount = 32;

	constexpr std::array<std::string_view, globalFeatureCount> globalFeatureNames{
		"volume", "surface_area", "compactness", "rectangularity", "diameter", "convexity", "eccentricity"};

	/// Seed used when none is given
	constexpr std::uint32_t defaultSeed = 0;

	/// @brief All features of one (normalized) shape
	struct FeatureVector
	{
		std::array<double, globalFeatureCount> global{};

		/// One histogram per stats::distributions(), in that order, each histogramBinCount bins summing to 1
		std::vector<std::vector<double>> histograms;
	};

	/// @brief Seed for the random sampling of one shape, from its file name and a base seed.
	/// A database shape gets the same seed whether it is stored or queried, so it has distance 0 to itself.
	[[nodiscard]] std::uint32_t shapeSeed(std::string_view fileName, std::uint32_t baseSeed = defaultSeed);

	/// @brief Computes all features of an already normalized mesh. The same mesh and seed always give the same result.
	[[nodiscard]] FeatureVector extractFeatures(const Mesh& mesh, std::uint32_t seed);
} // namespace querying

#endif // MULTIMEDIARETRIEVAL_FEATURE_VECTOR_HPP
