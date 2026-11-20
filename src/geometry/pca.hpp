//
// Created by dafitius on 09-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_PRINCIPLEAXES_HPP
#define MULTIMEDIARETRIEVAL_PRINCIPLEAXES_HPP

#define GLM_ENABLE_EXPERIMENTAL // For using glm::length2 (https://glm.g-truc.net/0.9.4/api/a00188.html)

#include "glm/detail/type_quat.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/quaternion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>

namespace mesh::pca
{
	/// @brief Covariance around the mean of the positions, so the model does not need to be centered first
	/// @return
	[[nodiscard]] inline glm::mat3 covariance(const std::span<const glm::vec3>& positions)
	{
		glm::mat3 cov(0.0f);

		if (positions.empty())
		{
			return cov;
		}

		glm::dvec3 sum(0.0);
		for (const auto& p : positions)
		{
			sum += glm::dvec3(p);
		}
		const auto mean = glm::vec3(sum / static_cast<double>(positions.size()));

		for (const auto& position : positions)
		{
			const auto p = position - mean;
			cov[0][0] += p.x * p.x;
			cov[0][1] += p.x * p.y;
			cov[0][2] += p.x * p.z;

			cov[1][0] += p.y * p.x;
			cov[1][1] += p.y * p.y;
			cov[1][2] += p.y * p.z;

			cov[2][0] += p.z * p.x;
			cov[2][1] += p.z * p.y;
			cov[2][2] += p.z * p.z;
		}

		cov /= positions.size();
		return cov;
	}

	// https://en.wikipedia.org/wiki/Eigenvalue_algorithm#Symmetric_3%C3%973_matrices
	[[nodiscard]] inline std::array<float, 3> eigenvalues(glm::mat3 covariance)
	{
		// Covariance matrix is symmetric.
		// [ a  b  c
		//   b  d  e
		//   c  e  f ]
		const float a = covariance[0][0];
		const float b = covariance[1][0];
		const float c = covariance[2][0];
		const float d = covariance[1][1];
		const float e = covariance[2][1];
		const float f = covariance[2][2];

		constexpr auto eps = std::numeric_limits<float>::epsilon();
		auto p1 = (b * b) + (c * c) + (e * e);
		if (p1 <= eps * ((a * a) + (d * d) + (f * f)))
		{
			std::array<float, 3> diagonal{a, d, f};
			std::ranges::sort(diagonal, std::greater{});
			return diagonal;
		}

		auto q = (a + d + f) / 3.0f;
		auto p2 = (a - q) * (a - q) + (d - q) * (d - q) + (f - q) * (f - q) + 2 * p1;
		p2 = std::max(0.0f, p2);
		auto p = std::sqrt(p2 / 6.0f);
		if (p <= eps * std::abs(q))
		{
			return {q, q, q};
		}


		auto B = (covariance - (glm::mat3(1.0) * q)) / p;
		auto r = glm::determinant(B) / 2;
		r = std::clamp(r, -1.0f, 1.0f);
		float phi = 0;
		if (r <= -1)
		{
			phi = glm::pi<float>() / 3.0f;
		}
		else if (r >= 1)
		{
			phi = 0.0f;
		}
		else
		{
			phi = std::acos(r) / 3.0f;
		}

		float eig1 = q + (2 * p * std::cos(phi));
		float eig3 = q + (2 * p * std::cos(phi + (2 * glm::pi<float>() / 3)));
		float eig2 = (3 * q) - eig1 - eig3;

		if (eig1 < eig2)
		{
			std::swap(eig1, eig2);
		}
		if (eig1 < eig3)
		{
			std::swap(eig1, eig3);
		}
		if (eig2 < eig3)
		{
			std::swap(eig2, eig3);
		}
		return {eig1, eig2, eig3};
	}

	//(A - λI) v = 0
	[[nodiscard]] inline glm::vec3 eigenvector(const glm::mat3& c, const float lambda)
	{
		glm::vec3 const r1(c[0][0] - lambda, c[1][0], c[2][0]);
		glm::vec3 const r2(c[0][1], c[1][1] - lambda, c[2][1]);
		glm::vec3 const r3(c[0][2], c[1][2], c[2][2] - lambda);

		glm::vec3 v = glm::cross(r1, r2);
		for (const auto& candidate : {glm::cross(r1, r3), glm::cross(r2, r3)})
		{
			if (glm::length2(candidate) > glm::length2(v))
			{
				v = candidate;
			}
		}

		constexpr auto eps = std::numeric_limits<float>::epsilon();
		const float rowScale = std::max({glm::length2(r1), glm::length2(r2), glm::length2(r3)});
		const float len2 = glm::length2(v);
		if (len2 <= eps * rowScale * rowScale)
		{
			return {1.0f, 0.0f, 0.0f}; // fallback
		}
		return v / std::sqrt(len2);
	}

	[[nodiscard]] static glm::quat compute(const std::span<const glm::vec3>& positions)
	{
		glm::mat3 const cov = covariance(positions);
		auto eigenvalues_ = eigenvalues(cov);

		const auto e0 = eigenvector(cov, eigenvalues_[0]);
		const auto e1 = eigenvector(cov, eigenvalues_[1]);
		const auto e2 = eigenvector(cov, eigenvalues_[2]);

		// Gram-Schmidt
		glm::vec3 const x = glm::normalize(e0);
		auto eps = std::numeric_limits<float>::epsilon();

		glm::vec3 y = e1 - x * glm::dot(x, e1);
		if (glm::length2(y) < eps)
		{
			y = e2 - x * glm::dot(x, e2);
		}
		if (glm::length2(y) < eps)
		{
			y = glm::cross(x, glm::vec3{0, 1, 0});
		}
		y = glm::normalize(y);

		glm::vec3 z = glm::cross(x, y); // Try to derive z from other components
		if (glm::length2(z) < eps)
		{
			z = glm::normalize(e2);
		}
		else
		{
			z = glm::normalize(z);
		}
		glm::mat3 const axes(x, y, z);

		if (std::isnan(glm::determinant(axes)))
		{
			std::cerr << "Something went wrong while calculating eigenvectors" << std::endl;
			std::cout << "eigenvalue[0] = " << eigenvalues_[0] << '\n';
			std::cout << "eigenvalue[1] = " << eigenvalues_[1] << '\n';
			std::cout << "eigenvalue[2] = " << eigenvalues_[2] << '\n';
			std::cout << "length2[e0] = " << length2(e0) << '\n';
			std::cout << "length2[e1] = " << length2(e1) << '\n';
			std::cout << "length2[e2] = " << length2(e2) << '\n';
			return glm::quat{1, 0, 0, 0};
		}
		return glm::normalize(glm::quat_cast(axes));
	}
} // namespace mesh::pca


#endif // MULTIMEDIARETRIEVAL_PRINCIPLEAXES_HPP
