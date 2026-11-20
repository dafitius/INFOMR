//
// Created by dafitius on 22-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_MESHDECIMATOR_HPP
#define MULTIMEDIARETRIEVAL_MESHDECIMATOR_HPP

#include "../mesh.hpp"
#include "ops.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mesh::ops::repair
{
	/// Uses greedy surface simplification with qem as described in:
	// https://doi.org/10.1145/258734.258849
	class MeshDecimator final {
	public:
		[[nodiscard]] static Mesh simplify(const Mesh& mesh, const float reductionFactor) {
			if (!std::isfinite(reductionFactor) || reductionFactor < 0.0f || reductionFactor > 1.0f) {
				throw std::invalid_argument("simplify: reductionFactor must be in [0, 1]");
			}

			if (reductionFactor == 0.0f || mesh.faceList().empty()) {
				return mesh;
			}
			return MeshDecimator(mesh, reductionFactor).applyCandidates();
		}

	private:
		using Index = FaceList::Index;
		using Position = glm::dvec3;

		struct Quadric {
			// Symmetric 4x4 matrix:
			// xx, xy, xz, xw,
			//     yy, yz, yw,
			//         zz, zw,
			//             ww.
			std::array<double, 10> quad{};

			double xx() const {
				return quad[0];
			};

			double xy() const {
				return quad[1];
			};

			double xz() const {
				return quad[2];
			};

			double xw() const {
				return quad[3];
			};

			double yx() const {
				return quad[1];
			};

			double yy() const {
				return quad[4];
			};

			double yz() const {
				return quad[5];
			};

			double yw() const {
				return quad[6];
			};

			double zx() const {
				return quad[2];
			};

			double zy() const {
				return quad[5];
			};

			double zz() const {
				return quad[7];
			};

			double zw() const {
				return quad[8];
			};

			double wx() const {
				return quad[3];
			};

			double wy() const {
				return quad[6];
			};

			double wz() const {
				return quad[8];
			};

			double ww() const {
				return quad[9];
			};

			[[nodiscard]] static Quadric fromFace(const Position& a, const Position& b, const Position& c) {
				Quadric q;
				const Position cross = glm::cross(b - a, c - a);
				const double area = glm::length(cross) * 0.5;
				const Position normal = glm::normalize(cross);

				q.addPlane(normal, -glm::dot(normal, a), area);
				return q;
			}

			void add(const Quadric& other) {
				for (std::size_t i = 0; i < quad.size(); ++i) {
					quad[i] += other.quad[i];
				}
			}

			void addPlane(const Position& n, const double d, const double weight) {
				const auto a = n.x;
				const auto b = n.y;
				const auto c = n.z;

				quad[0] += weight * a * a;
				quad[1] += weight * a * b;
				quad[2] += weight * a * c;
				quad[3] += weight * a * d;
				quad[4] += weight * b * b;
				quad[5] += weight * b * c;
				quad[6] += weight * b * d;
				quad[7] += weight * c * c;
				quad[8] += weight * c * d;
				quad[9] += weight * d * d;
			}

			/// returns the sum of squared distances to the planes.
			/// Measures how much the position is offset from the surrounding face planes.
			/// Lower values mean a better fit.
			[[nodiscard]] double evaluate(const Position& p) const {
				const auto x = p.x;
				const auto y = p.y;
				const auto z = p.z;

				return (xx() * x * x) + (xy() * x * y) + (xz() * x * z) + (xw() * x) + (yx() * y * x) + (yy() * y * y) + (yz() * y * z) + (yw() * y) +
					   (zx() * z * x) + (zy() * z * y) + (zz() * z * z) + (zw() * z) + (wx() * x) + (wy() * y) + (wz() * z) + ww();
			}

			/// Finds the position that best preserves the surrounding mesh shape.
			/// by computing the position that minimizes the quadric error.
			/// Returns false if no finite, unique solution can be found.
			[[nodiscard]] std::optional<Position> solveOptimalPosition() const {
				const double a00 = xx();
				const double a01 = xy();
				const double a02 = xz();
				const double a11 = yy();
				const double a12 = yz();
				const double a22 = zz();

				const double b0 = xw();
				const double b1 = yw();
				const double b2 = zw();

				const double c00 = (a11 * a22) - (a12 * a12);
				const double c01 = (a02 * a12) - (a01 * a22);
				const double c02 = (a01 * a12) - (a02 * a11);
				const double c11 = (a00 * a22) - (a02 * a02);
				const double c12 = (a01 * a02) - (a00 * a12);
				const double c22 = (a00 * a11) - (a01 * a01);

				const double determinant = (a00 * c00) + (a01 * c01) + (a02 * c02);

				if (std::abs(determinant) <= epsilon) {
					return std::nullopt;
				}

				const double inverseDeterminant = 1.0 / determinant;

				auto result = Position{-((c00 * b0) + (c01 * b1) + (c02 * b2)) * inverseDeterminant,
									   -((c01 * b0) + (c11 * b1) + (c12 * b2)) * inverseDeterminant,
									   -((c02 * b0) + (c12 * b1) + (c22 * b2)) * inverseDeterminant};

				if (std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z)) {
					return std::make_optional(result);
				}
				return std::nullopt;
			}
		};

		struct Triangle {
			Index a, b, c;
			bool alive = true;

			std::array<std::reference_wrapper<Index>, 3> parts() {
				return {std::ref(a), std::ref(b), std::ref(c)};
			}
		};

		struct EdgeKey {
			Index a, b;

			EdgeKey(Index x, Index y)
				: a(std::min(x, y))
				, b(std::max(x, y)) {}

			bool operator==(const EdgeKey&) const = default;
		};

		struct Candidate {
			double error;
			Index a, b;
			Position position;
			std::size_t versionA, versionB;
		};

		struct CandidateGreater {
			bool operator()(const Candidate& lhs, const Candidate& rhs) const {
				return lhs.error > rhs.error;
			}
		};

		using CandidateQueue = std::priority_queue<Candidate, std::vector<Candidate>, CandidateGreater>;

		static constexpr double boundaryWeight = 1000.0;
		static constexpr double epsilon = std::numeric_limits<double>::epsilon();

		const Mesh& source_;
		std::size_t targetVertices_;

		std::vector<Position> positions_;
		std::vector<Triangle> triangles_;
		std::vector<Quadric> quadrics_;
		std::vector<std::unordered_set<Index>> neighbors_;
		std::vector<std::vector<Index>> vertexTriangles_;
		std::vector<std::size_t> versions_;
		std::vector<bool> alive_;
		CandidateQueue candidates_;
		std::size_t aliveVertices_{};

		MeshDecimator(const Mesh& mesh, const float reductionFactor)
			: source_(mesh)
			, targetVertices_(std::max<std::size_t>(
				  1, static_cast<std::size_t>(std::ceil(mesh.positions().size() * (1.0 - static_cast<double>(reductionFactor))))))
			, aliveVertices_(mesh.positions().size()) {
			initialize();
		}

		[[nodiscard]] Mesh applyCandidates() {
			while (aliveVertices_ > targetVertices_ && !candidates_.empty()) {
				const Candidate candidate = candidates_.top();
				candidates_.pop();

				if (!isCurrent(candidate)) {
					continue;
				}

				if (isUnsafeCollapse(candidate)) {
					continue;
				}

				applyCollapse(candidate);
			}

			return buildMesh();
		}

		void initialize() {
			const auto& inputPositions = source_.positions();
			const auto& inputFaces = source_.faceList();

			positions_.reserve(inputPositions.size());
			for (const auto& p : inputPositions) {
				positions_.emplace_back(p.x, p.y, p.z);
			}

			const std::size_t count = positions_.size();

			quadrics_.resize(count);
			neighbors_.resize(count);
			vertexTriangles_.resize(count);
			versions_.resize(count, 0);
			alive_.resize(count, true);

			for (const auto face : inputFaces.all()) {
				if (face.size() != 3) {
					throw std::invalid_argument("Mesh must be a triangle mesh");
				}

				const Index a = face[0];
				const Index b = face[1];
				const Index c = face[2];

				if (a >= count || b >= count || c >= count) {
					throw std::invalid_argument("invalid vertex index");
				}

				if (a == b || b == c || c == a) {
					continue;
				}

				const Index ti = triangles_.size();
				triangles_.push_back({.a = a, .b = b, .c = c, .alive = true});
				vertexTriangles_[a].push_back(ti);
				vertexTriangles_[b].push_back(ti);
				vertexTriangles_[c].push_back(ti);


				const Quadric quad = Quadric::fromFace(positions_[a], positions_[b], positions_[c]);

				quadrics_[a].add(quad);
				quadrics_[b].add(quad);
				quadrics_[c].add(quad);
			}


			// Build neighbours
			for (auto& neighbors : neighbors_) {
				neighbors.clear();
			}

			for (const auto& t : triangles_) {
				if (!t.alive) {
					continue;
				}

				neighbors_[t.a].insert(t.b);
				neighbors_[t.a].insert(t.c);
				neighbors_[t.b].insert(t.a);
				neighbors_[t.b].insert(t.c);
				neighbors_[t.c].insert(t.a);
				neighbors_[t.c].insert(t.b);
			}

			addBoundaryConstraints();

			for (Index a = 0; a < neighbors_.size(); ++a) {
				for (const Index b : neighbors_[a]) {
					if (a < b) {
						enqueueCollapse(a, b);
					}
				}
			}
		}

		void addBoundaryConstraints() {
			std::unordered_map<EdgeKey, std::size_t, DirectedEdgeKeyHash> edgeCounts;

			for (const auto& t : triangles_) {
				edgeCounts[EdgeKey(t.a, t.b)] += 1;
				edgeCounts[EdgeKey(t.b, t.c)] += 1;
				edgeCounts[EdgeKey(t.c, t.a)] += 1;
			}

			auto addConstraint = [this](const Index a, const Index b, const Index third) {
				const Position edge = positions_[b] - positions_[a]; // direction of the edge
				const Position faceNormal = glm::cross(edge, positions_[third] - positions_[a]);
				const Position normal = glm::normalize(glm::cross(edge, faceNormal));

				Quadric quad;
				const auto d = -glm::dot(normal, positions_[a]); // distance or offset to origin
				quad.addPlane(normal, d, boundaryWeight);

				quadrics_[a].add(quad);
				quadrics_[b].add(quad);
			};

			for (const auto& t : triangles_) {
				if (edgeCounts[EdgeKey(t.a, t.b)] == 1) {
					addConstraint(t.a, t.b, t.c);
				}

				if (edgeCounts[EdgeKey(t.b, t.c)] == 1) {
					addConstraint(t.b, t.c, t.a);
				}

				if (edgeCounts[EdgeKey(t.c, t.a)] == 1) {
					addConstraint(t.c, t.a, t.b);
				}
			}
		}

		[[nodiscard]] Candidate makeCandidate(const Index a, const Index b) const {
			Quadric q = quadrics_[a];
			q.add(quadrics_[b]);

			const auto position = q.solveOptimalPosition().value_or([&] { return (positions_[a] + positions_[b]) * 0.5; }());
			return {
				.error = std::max(0.0, q.evaluate(position)),
				.a = a,
				.b = b,
				.position = position,
				.versionA = versions_[a],
				.versionB = versions_[b],
			};
		}

		void enqueueCollapse(Index a, Index b) {
			if (a == b || !alive_[a] || !alive_[b]) {
				return;
			}

			if (a > b) {
				std::swap(a, b);
			}

			candidates_.push(makeCandidate(a, b));
		}

		[[nodiscard]] bool isCurrent(const Candidate& candidate) const {
			const auto [a, b] = std::pair{candidate.a, candidate.b};
			return alive_[a] && alive_[b] && versions_[a] == candidate.versionA && versions_[b] == candidate.versionB && neighbors_[a].contains(b);
		}

		[[nodiscard]] bool isUnsafeCollapse(const Candidate& candidate) const {
			const auto [a, b] = std::pair{candidate.a, candidate.b};

			const auto point = [&](const Index v) -> Position { return (v == a || v == b) ? candidate.position : positions_[v]; };

			const auto wouldFlip = [&](const Index ti) {
				const Triangle& t = triangles_[ti];
				if (!t.alive) {
					return false;
				}

				const bool containsA = t.a == a || t.b == a || t.c == a;
				const bool containsB = t.a == b || t.b == b || t.c == b;

				if (containsA == containsB) {
					return false;
				}

				const Position oldNormal = glm::cross(positions_[t.b] - positions_[t.a], positions_[t.c] - positions_[t.a]);
				const Position newNormal = glm::cross(point(t.b) - point(t.a), point(t.c) - point(t.a));
				return glm::length(newNormal) <= epsilon || glm::dot(oldNormal, newNormal) <= 0.0;
			};

			return std::ranges::any_of(vertexTriangles_[a], wouldFlip) || std::ranges::any_of(vertexTriangles_[b], wouldFlip);
		}

		void applyCollapse(const Candidate& candidate) {
			const Index a = candidate.a;
			const Index b = candidate.b;

			positions_[a] = candidate.position;
			quadrics_[a].add(quadrics_[b]);

			++versions_[a];
			++versions_[b];
			alive_[b] = false;
			--aliveVertices_;

			for (const auto& ti : vertexTriangles_[b]) {
				auto& triangle = triangles_[ti];
				for (Index& vertex : triangle.parts()) {
					if (vertex == b) {
						vertex = a;
					}
				}

				if (triangle.a == triangle.b || triangle.b == triangle.c || triangle.c == triangle.a) {
					triangle.alive = false;
				} else {
					vertexTriangles_[a].push_back(ti);
				}
			}

			vertexTriangles_[b].clear();
			std::erase_if(vertexTriangles_[a], [&](const Index ti) { return !triangles_[ti].alive; });

			// patch neighbours
			for (const Index n : neighbors_[b]) {
				neighbors_[n].erase(b);
				if (n != a) {
					neighbors_[n].insert(a);
					neighbors_[a].insert(n);
				}
			}
			neighbors_[a].erase(b);
			neighbors_[b].clear();

			// Only candidates touching a have changed costs or topology.
			for (const Index neighbor : neighbors_[a]) {
				enqueueCollapse(a, neighbor);
			}
		}

		[[nodiscard]] Mesh buildMesh() const {
			constexpr Index invalid = std::numeric_limits<Index>::max();
			std::vector remap(positions_.size(), invalid);
			std::vector<glm::vec3> positions;
			positions.reserve(aliveVertices_);

			for (Index i = 0; i < positions_.size(); ++i) {
				if (!alive_[i]) {
					continue;
				}

				remap[i] = positions.size();

				positions.emplace_back(static_cast<float>(positions_[i].x), static_cast<float>(positions_[i].y), static_cast<float>(positions_[i].z));
			}

			FaceList faces;
			faces.reserve(triangles_.size(), triangles_.size() * 3);

			std::unordered_set<std::array<Index, 3>, TriangleHash> seen;

			for (const auto& t : triangles_) {
				if (!t.alive) {
					continue;
				}

				std::array face{remap[t.a], remap[t.b], remap[t.c]};

				auto key = face;
				std::ranges::sort(key);

				if (seen.insert(key).second) {
					faces.push_back(face);
				}
			}

			return source_.withPositions(positions).withFaces(faces);
		}

		struct TriangleHash {
			std::size_t operator()(const std::array<Index, 3>& t) const noexcept {
				return std::hash<Index>{}(t[0]) << static_cast<uint32_t>(0) ^ std::hash<Index>{}(t[1]) << static_cast<uint32_t>(1) ^
					   std::hash<Index>{}(t[2]) << static_cast<uint32_t>(2);
			}
		};
	};
} // namespace mesh::ops::repair


#endif // MULTIMEDIARETRIEVAL_MESHDECIMATOR_HPP
