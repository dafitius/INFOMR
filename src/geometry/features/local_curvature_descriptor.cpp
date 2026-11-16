//
// Created by dafitius on 13-11-2026.
//

#include "../mesh.hpp"
#include "glm/vec3.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace
{
	using Curvatures = std::pair<double, double>;
	constexpr auto eps = std::numeric_limits<double>::epsilon();

	// "local quadratic surface to the centroids of a face and its neighboring faces using least-squares regression" -
	// https://hal.science/hal-00725647/file/3dpvt02.pdf
	std::optional<Curvatures>
	estimatePrincipalCurvatures(const std::vector<glm::dvec3>& points, const glm::dvec3& origin, const glm::dvec3& normal, const glm::dvec3& tangent)
	{
		const auto bitangent = glm::cross(normal, tangent);

		Eigen::MatrixXd A(points.size(), 6);
		Eigen::VectorXd b(points.size());

		for (std::size_t i = 0; i < points.size(); ++i)
		{
			const auto delta = points[i] - origin;

			const double x = glm::dot(delta, tangent);
			const double y = glm::dot(delta, bitangent);
			const double z = glm::dot(delta, normal);

			A.row(i) << x * x, y * y, x * y, x, y, 1.0;
			b(i) = z;
		}

		const Eigen::VectorXd c = A.colPivHouseholderQr().solve(b);

		if (!c.allFinite())
		{
			return std::nullopt;
		}

		const double a = c(0);
		const double b2 = c(1);
		const double crossTerm = c(2);
		const double d = c(3);
		const double e = c(4);

		// First and second fundamental forms.
		const double E = 1.0 + d * d;
		const double F = d * e;
		const double G = 1.0 + e * e;

		const double scale = std::sqrt(E + e * e);

		const double L = 2.0 * a / scale;
		const double M = crossTerm / scale;
		const double N = 2.0 * b2 / scale;

		// Principal curvatures are eigenvalues of I^-1 * II.
		Eigen::Matrix2d I;
		I << E, F, F, G;

		Eigen::Matrix2d II;
		II << L, M, M, N;

		Eigen::GeneralizedSelfAdjointEigenSolver const solver(II, I);

		if (solver.info() != Eigen::Success)
		{
			return std::nullopt;
		}

		const auto& eigenvalues = solver.eigenvalues();

		return Curvatures{eigenvalues(1), eigenvalues(0)};
	}

	// From https://hal.science/hal-00725647/file/3dpvt02.pdf
	// Even though the paper says this function returns in range [0,1] this is not true. It can return up to -1, making the range [-1,1]
	std::optional<double> calculateCurvatureIndex(const double k1, const double k2)
	{
		const double denominator = k1 - k2;
		if (std::abs(denominator) < eps)
		{
			if (std::abs(k1) < eps)
			{
				return std::nullopt;
			}
			return std::copysign(1.0, k1);
		}

		return 2.0 / std::numbers::pi * std::atan((k1 + k2) / denominator);
	}
} // namespace

namespace mesh::features::local_curvature_descriptor
{
	std::vector<std::optional<Curvatures>> calcPrincipalCurvatures(const Mesh& mesh, const size_t maxDepth)
	{
		const auto& positions = mesh.positions();
		const auto& faces = mesh.faces().list();

		if (mesh.faceType() != FaceSet::FaceType::TRI || faces.empty())
		{
			return {};
		}

		const std::size_t numFaces = faces.size();

		// Calculate face centroids and normals.
		std::vector<glm::dvec3> centroids(numFaces);
		std::vector<glm::dvec3> normals(numFaces);

		for (std::size_t f = 0; f < numFaces; ++f)
		{
			const auto& face = faces[f];

			const glm::dvec3 a = positions[face[0]];
			const glm::dvec3 b = positions[face[1]];
			const glm::dvec3 c = positions[face[2]];

			centroids[f] = (a + b + c) / 3.0;

			const auto normal = glm::cross(b - a, c - a);
			const double length = glm::length(normal);

			if (length > eps)
				normals[f] = normal / length;
		}

		// Build face adjacency from halfedges.
		auto adjacency = mesh.faces().faceAdjacency();

		std::vector<std::optional<Curvatures>> curvatureValues(numFaces);
		std::vector<std::size_t> visitedGeneration(numFaces, 0);
		std::vector<std::size_t> depth(numFaces);
		std::vector<std::size_t> neighborhood;
		std::vector<glm::dvec3> points;
		neighborhood.reserve(std::min(numFaces, std::size_t{512}));
		points.reserve(std::min(numFaces, std::size_t{512}));

		for (std::size_t f = 0; f < numFaces; ++f)
		{
			if (glm::length(normals[f]) < eps)
				continue;

			neighborhood.clear();
			neighborhood.push_back(f);
			visitedGeneration[f] = f + 1;
			depth[f] = 0;

			// Find the face neighborhood using BFS.
			for (std::size_t i = 0; i < neighborhood.size(); ++i)
			{
				const auto current = neighborhood[i];

				if (depth[current] >= maxDepth)
					continue;

				for (const auto neighbor : adjacency[current])
				{
					if (visitedGeneration[neighbor] == f + 1)
					{
						continue;
					}

					visitedGeneration[neighbor] = f + 1;
					depth[neighbor] = depth[current] + 1;
					neighborhood.push_back(neighbor);
				}
			}

			// Six coefficients require at least six independent samples.
			if (neighborhood.size() < 6)
				continue;

			points.clear();
			for (const auto neighbor : neighborhood)
				points.push_back(centroids[neighbor]);

			// Construct a local tangent frame.
			const auto& face = faces[f];

			const glm::dvec3 a = positions[face[0]];
			const glm::dvec3 b = positions[face[1]];

			const auto normal = normals[f];
			const auto edge = b - a;

			const auto projectedEdge = edge - glm::dot(edge, normal) * normal;

			const double edgeLength = glm::length(projectedEdge);

			if (edgeLength < eps)
			{
				continue;
			}

			const auto tangent = projectedEdge / edgeLength;
			curvatureValues[f] = estimatePrincipalCurvatures(points, centroids[f], normal, tangent);
		}
		return curvatureValues;
	}

	template <typename Projection>
	std::vector<float> principalCurvatureFaceValues(const Mesh& mesh, const size_t depth, Projection projection, const bool normalize = true)
	{
		std::vector<float> values;
		values.reserve(mesh.numFaces());
		const float invalid = std::numeric_limits<float>::quiet_NaN();
		double maxCurvature = 0.0;

		for (const auto& candidates : calcPrincipalCurvatures(mesh, depth))
		{
			if (!candidates.has_value())
			{
				values.push_back(invalid);
				continue;
			}

			auto [k1, k2] = *candidates;
			const std::optional<double> value = projection(k1, k2);

			if (!value.has_value() || !std::isfinite(*value))
			{
				values.push_back(invalid);
				continue;
			}

			values.push_back(static_cast<float>(*value));
			maxCurvature = std::max(maxCurvature, std::abs(*value));
		}

		if (maxCurvature > 0.0 && normalize)
		{
			for (auto& value : values)
			{
				if (std::isfinite(value))
					value /= static_cast<float>(maxCurvature);
			}
		}
		return values;
	}

	std::vector<float> curvatureIndex(const Mesh& mesh, const size_t radius, const size_t numBins)
	{
		std::vector histogram(numBins, 0.0F);

		const auto values = principalCurvatureFaceValues(mesh, radius, [](double k1, double k2) { return calculateCurvatureIndex(k1, k2); }, false);

		for (const auto value : values)
		{
			const double normalized = (value + 1.0) * 0.5;
			const auto bin = std::min(static_cast<std::size_t>(normalized * numBins), numBins - 1);

			histogram[bin] += 1.0F;
		}

		if (!values.empty())
		{
			for (auto& bin : histogram)
				bin /= static_cast<float>(values.size());
		}

		return histogram;
	}

	std::vector<float> minCurvatureFaceValues(const Mesh& mesh, const size_t radius)
	{
		return principalCurvatureFaceValues(mesh, radius, [](double k1, double _) { return abs(k1); });
	}

	std::vector<float> maxCurvatureFaceValues(const Mesh& mesh, const size_t radius)
	{
		return principalCurvatureFaceValues(mesh, radius, [](double _, double k2) { return abs(k2); });
	}

	std::vector<float> curvatureIndexFaceValues(const Mesh& mesh, const size_t radius)
	{
		return principalCurvatureFaceValues(
			mesh, radius,
			[](double k1, double k2)
			{
				const auto value = calculateCurvatureIndex(k1, k2);
				if (value.has_value())
				{
					return std::make_optional((*value + 1.0) * 0.5);
				}
				return value;
			});
	}
} // namespace mesh::features::local_curvature_descriptor