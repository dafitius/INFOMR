//
// Created by DanioHans on 27-10-2026.
//

#include "distributions.hpp"

#include "../geometry/features/features.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numbers>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace stats
{
	namespace
	{
		constexpr double maxD1 = 0.8;
		constexpr double maxD2 = 1.2;
		constexpr double maxD3 = 0.65;
		constexpr double maxD4 = 0.35;
		constexpr size_t curvatureNeighborhoodDepth = 7;

		std::vector<float> curvatureIndexSamples(const Mesh& mesh) {
			return mesh::features::local_curvature_descriptor::curvatureIndexFaceValues(mesh, curvatureNeighborhoodDepth);
		}

		void writeCsvField(std::ostream& output, const std::string_view value) {
			output << '"';
			for (const char character : value) {
				if (character == '"') {
					output << '"';
				}
				output << character;
			}
			output << '"';
		}
	} // namespace

	std::span<const Distribution> distributions() {
		namespace descriptors = mesh::features::distribution_descriptors;

		static constexpr std::array all{
			Distribution{"a3", "Angle between three random vertices", BinRange{.min = 0.0, .max = std::numbers::pi}, descriptors::A3},
			Distribution{"d1", "Distance between the barycenter and a random vertex", BinRange{.min = 0.0, .max = maxD1}, descriptors::D1},
			Distribution{"d2", "Distance between two random vertices", BinRange{.min = 0.0, .max = maxD2}, descriptors::D2},
			Distribution{"d3", "Square root of the area of the triangle between three random vertices", BinRange{.min = 0.0, .max = maxD3},
						 descriptors::D3},
			Distribution{"d4", "Cube root of the volume of the tetrahedron between four random vertices", BinRange{.min = 0.0, .max = maxD4},
						 descriptors::D4},
			Distribution{"curvature", "Local curvature index", BinRange{.min = 0.0, .max = 1.0}, curvatureIndexSamples},
		};
		return all;
	}

	std::optional<Distribution> findDistribution(const std::string_view name) {
		const auto all = distributions();
		const auto found = std::ranges::find(all, name, &Distribution::name);
		return found != all.end() ? std::make_optional(*found) : std::nullopt;
	}

	std::vector<double> binHistogram(const std::span<const float> samples, const BinRange range, const size_t numBins) {
		std::vector histogram(numBins, 0.0);
		if (numBins == 0 || range.width() <= 0.0) {
			return histogram;
		}

		size_t total = 0;
		for (const auto sample : samples) {
			if (!std::isfinite(sample)) {
				continue;
			}

			const auto position = std::clamp((static_cast<double>(sample) - range.min) / range.width(), 0.0, 1.0);
			const auto bin = std::min(static_cast<size_t>(position * static_cast<double>(numBins)), numBins - 1);
			histogram[bin] += 1.0;
			++total;
		}

		if (total > 0) {
			for (auto& value : histogram) {
				value /= static_cast<double>(total);
			}
		}
		return histogram;
	}

	HistogramCsvResult writePerShapeHistogramsCsv(const ShapeDb& database, const std::span<const ShapeRef> shapes, const mesh::Pipeline& pipeline,
												  const std::filesystem::path& path, const size_t numBins, const std::atomic<bool>* cancel,
												  const HistogramProgressCallback& onProgress) {
		HistogramCsvResult result;
		if (numBins == 0) {
			std::cerr << "Unable to export histograms with zero bins\n";
			return result;
		}

		std::ofstream output(path);
		if (!output.is_open()) {
			std::cerr << "Unable to write file: " << path.string() << '\n';
			return result;
		}

		output << "# pipeline: " << pipeline.label() << '\n';
		output << "class,file,descriptor,bin,range_min,range_max,frequency,sample_count\n";

		const auto allDistributions = distributions();
		for (size_t shapeIndex = 0; shapeIndex < shapes.size(); ++shapeIndex) {
			if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
				return result;
			}

			const auto& shape = shapes[shapeIndex];
			const auto shapePath = database.resourcePath(shape.className, shape.fileName);
			bool shapeWritten = false;
			if (shapePath.extension() == ".obj") {
				try {
					if (auto loaded = Mesh::fromObj(shapePath)) {
						const auto transformed = pipeline.apply(*loaded);
						const auto& cumulativeAreas = transformed.cumulativeFaceAreas();
						if (!cumulativeAreas.empty() && std::isfinite(cumulativeAreas.back()) && cumulativeAreas.back() > 0.0) {
							std::vector<std::vector<double>> histograms;
							std::vector<std::size_t> sampleCounts;
							histograms.reserve(allDistributions.size());
							sampleCounts.reserve(allDistributions.size());
							for (const auto& distribution : allDistributions) {
								const auto samples = distribution.sample(transformed);
								histograms.push_back(binHistogram(samples, distribution.range, numBins));
								sampleCounts.push_back(static_cast<std::size_t>(
									std::ranges::count_if(samples, [](const float sample) { return std::isfinite(sample); })));
							}

							for (std::size_t distributionIndex = 0; distributionIndex < allDistributions.size(); ++distributionIndex) {
								const auto& distribution = allDistributions[distributionIndex];
								const auto& histogram = histograms[distributionIndex];
								const double binWidth = distribution.range.width() / static_cast<double>(numBins);
								for (size_t bin = 0; bin < histogram.size(); ++bin) {
									writeCsvField(output, shape.className);
									output << ',';
									writeCsvField(output, shape.fileName);
									output << ',' << distribution.name << ',' << bin << ','
										   << std::format("{}", distribution.range.min + static_cast<double>(bin) * binWidth) << ','
										   << std::format("{}", distribution.range.min + static_cast<double>(bin + 1) * binWidth) << ','
										   << std::format("{}", histogram[bin]) << ',' << sampleCounts[distributionIndex] << '\n';
									++result.rowsWritten;
								}

								if (!output.good()) {
									std::cerr << "Unable to write file: " << path.string() << '\n';
									return result;
								}
							}
							result.histogramCount += allDistributions.size();
							++result.shapesWritten;
							shapeWritten = true;
						} else {
							std::cerr << "Cannot sample mesh without positive surface area: " << shapePath.string() << '\n';
						}
					} else {
						std::cerr << "Failed to load mesh: " << shapePath.string() << '\n';
					}
				}
				catch (const std::exception& error) {
					std::cerr << "Failed to process " << shapePath.string() << ": " << error.what() << '\n';
				}
			} else {
				std::cerr << "Unsupported mesh format: " << shapePath.string() << '\n';
			}

			if (!shapeWritten) {
				result.failed.push_back(shapePath);
			}
			if (onProgress) {
				onProgress(shapeIndex + 1, shapes.size());
			}
		}

		result.written = output.good();
		if (!result.written) {
			std::cerr << "Unable to write file: " << path.string() << '\n';
		}
		return result;
	}

	std::optional<double> earthMoversDistance(const std::span<const double> first, const std::span<const double> second) {
		if (first.size() != second.size()) {
			return std::nullopt;
		}

		const auto firstMass = std::accumulate(first.begin(), first.end(), 0.0);
		const auto secondMass = std::accumulate(second.begin(), second.end(), 0.0);
		if (firstMass <= 0.0 || secondMass <= 0.0) {
			return std::nullopt;
		}
		if (first.size() < 2) {
			return 0.0;
		}

		double cumulativeDifference = 0.0;
		double movedMass = 0.0;
		for (size_t bin = 0; bin + 1 < first.size(); ++bin) {
			cumulativeDifference += (first[bin] / firstMass) - (second[bin] / secondMass);
			movedMass += std::abs(cumulativeDifference);
		}
		return movedMass / static_cast<double>(first.size() - 1);
	}
} // namespace stats
