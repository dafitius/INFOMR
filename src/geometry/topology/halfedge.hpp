//
// Created by dafitius on 07-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
#define MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H

#include "../face_list.hpp"
#include "common.hpp"


#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace half_edge
{
	using HalfedgeRef = size_t;
	using FaceRef = size_t;
	using VertexRef = size_t;
	using Face = std::vector<VertexRef>;

	struct Halfedge {
		VertexRef vertex{};
		FaceRef face{};
		HalfedgeRef next{};
		HalfedgeRef prev{};
		std::optional<HalfedgeRef> opposite;
	};

	struct EdgeKey {
		VertexRef a;
		VertexRef b;

		bool operator==(const EdgeKey&) const = default;

		EdgeKey canonize() const {
			return {.a = std::min(a, b), .b = std::max(a, b)};
		}
	};

	using EdgeToFaceMap = std::unordered_map<EdgeKey, std::vector<FaceRef>, UndirectedEdgeKeyHash>;

	struct HalfEdgeSet {
	private:
		std::vector<Halfedge> halfedges_;
		std::vector<std::optional<HalfedgeRef>> vertex_halfedge_; // vertex idx -> halfedge
		std::vector<std::optional<HalfedgeRef>> face_halfedge_;	  // face idx -> halfedge
		Lazy<EdgeToFaceMap> edgeToFace_;
		Lazy<std::vector<std::vector<HalfedgeRef>>> vertexHalfedges_; // vertex idx -> incident halfedges
		Lazy<std::vector<std::vector<EdgeKey>>> vertexEdges_;		  // vertex idx -> incident edges

	public:
		[[nodiscard]] std::span<const Halfedge> halfedges() const {
			return std::span{halfedges_};
		}

		[[nodiscard]] auto boundaryHalfedges() const {
			return halfedges_ | std::views::filter([this](const Halfedge& he) { return isBoundaryHalfedge(he); });
		}

		[[nodiscard]] std::optional<Halfedge> getHalfedgeFromVertex(const VertexRef vertex) const {
			const auto ref = vertex_halfedge_[vertex];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		[[nodiscard]] std::optional<Halfedge> getHalfedgeFromFace(const FaceRef face) const {
			const auto ref = face_halfedge_[face];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		[[nodiscard]] std::optional<HalfedgeRef> halfedgeRefFromVertex(const VertexRef vertex) const {
			return vertex_halfedge_[vertex];
		}

		[[nodiscard]] std::optional<HalfedgeRef> halfedgeRefFromFace(const FaceRef face) const {
			return face_halfedge_[face];
		}

		[[nodiscard]]
		bool isNonManifoldHalfedge(const Halfedge& edge) const {
			const auto& edgeFace = this->edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });
			const auto from = halfedges()[edge.prev].vertex;
			const auto to = edge.vertex;

			const EdgeKey key{from, to};
			const auto& edgeFaces = edgeFace.at(key.canonize());
			return edgeFaces.size() > 2 && !edge.opposite.has_value();
		}

		[[nodiscard]]
		bool isBoundaryHalfedge(const Halfedge& edge) const {
			const auto& edgeFace = this->edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });
			const auto from = halfedges()[edge.prev].vertex;
			const auto to = edge.vertex;

			const EdgeKey key{from, to};
			const auto& edgeFaces = edgeFace.at(key.canonize());
			return edgeFaces.size() == 1 && !edge.opposite.has_value();
		}

		[[nodiscard]]
		bool isBoundaryHalfedge(const HalfedgeRef edge) const {
			const auto& he = halfedges()[edge];
			return isBoundaryHalfedge(he);
		}

		[[nodiscard]] bool isClosed() const {
			return std::ranges::all_of(halfedges_, [](const Halfedge& he) { return he.opposite.has_value(); });
		}

		[[nodiscard]] std::vector<VertexRef> orderedVertexNeighbors(const VertexRef v) const {
			if (!isClosed()) {
				std::cerr << "Tried to run vertexRing on unclosed mesh, this is unsupported!" << '\n';
				return std::vector<VertexRef>{};
			}

			std::vector<VertexRef> neighbors;

			if (v >= vertex_halfedge_.size() || !vertex_halfedge_[v].has_value()) {
				return neighbors;
			}

			const auto start = *vertex_halfedge_[v];
			auto current = start;
			for (std::size_t i = 0; i < halfedges_.size(); ++i) {
				neighbors.push_back(next(current).vertex);
				current = opposite(current).prev;
				if (current == start) {
					break;
				}
			}

			return neighbors;
		}

		static HalfEdgeSet build(const FaceList& faces, const std::size_t numVertices) {
			HalfEdgeSet topo;
			topo.vertex_halfedge_.assign(numVertices, std::nullopt);
			topo.face_halfedge_.resize(faces.size(), std::nullopt);

			std::unordered_map<EdgeKey, std::vector<HalfedgeRef>, UndirectedEdgeKeyHash> edgeMap;
			edgeMap.reserve(faces.indices().size());

			for (const auto& [faceIndex, face] : faces.all() | std::views::enumerate) {
				const auto f = static_cast<std::size_t>(faceIndex);
				const auto faceStart = topo.halfedges_.size();
				topo.face_halfedge_[f] = faceStart;

				const std::size_t numFaceVertices = face.size();
				if (numFaceVertices == 0) {
					continue;
				}
				topo.halfedges_.resize(topo.halfedges_.size() + numFaceVertices);

				for (std::size_t i = 0; i < numFaceVertices; ++i) {
					const HalfedgeRef edgeIndex = faceStart + i;
					const HalfedgeRef nextIndex = faceStart + ((i + 1) % numFaceVertices);
					const HalfedgeRef prevIndex = faceStart + ((i + numFaceVertices - 1) % numFaceVertices);

					const auto origin = static_cast<VertexRef>(face[i]);
					const auto dest = static_cast<VertexRef>(face[(i + 1) % numFaceVertices]);

					topo.halfedges_[edgeIndex] = Halfedge{
						.vertex = dest,
						.face = static_cast<size_t>(f),
						.next = nextIndex,
						.prev = prevIndex,
						.opposite = std::nullopt,
					};

					if (topo.vertex_halfedge_[dest] == std::nullopt) {
						topo.vertex_halfedge_[dest] = edgeIndex;
					}

					const EdgeKey key{origin, dest};
					edgeMap[key.canonize()].push_back(edgeIndex);
				}
			}

			for (const auto& halfedges : edgeMap | std::views::values) {
				if (halfedges.size() != 2) {
					continue;
				}
				const HalfedgeRef a = halfedges[0];
				const HalfedgeRef b = halfedges[1];

				topo.halfedges_[a].opposite = b;
				topo.halfedges_[b].opposite = a;
			}

			for (HalfedgeRef h = 0; h < topo.halfedges_.size(); ++h) {
				const auto& he = topo.halfedges_[h];
				if (!he.opposite.has_value()) {
					topo.vertex_halfedge_[he.vertex] = h;
				}
			}

			return topo;
		}

		[[nodiscard]]
		std::vector<std::vector<std::size_t>> boundaryLoops() const {
			const auto& hes = halfedges();

			auto nextBoundary = [&](const HalfedgeRef edgeIndex) -> std::optional<HalfedgeRef> {
				auto nextEdgeIndex = hes[edgeIndex].next;

				for (std::size_t guard = 0; guard < hes.size(); ++guard) {
					if (isBoundaryHalfedge(nextEdgeIndex)) {
						return nextEdgeIndex;
					}

					if (!has_opposite(nextEdgeIndex)) {
						return std::nullopt;
					}

					nextEdgeIndex = opposite(nextEdgeIndex).next;
				}

				return std::nullopt;
			};

			std::vector<bool> seen(hes.size(), false);
			std::vector<std::vector<std::size_t>> loops;

			for (HalfedgeRef start = 0; start < hes.size(); ++start) {
				if (!isBoundaryHalfedge(start) || seen[start]) {
					continue;
				}

				std::vector<std::size_t> loop;
				auto h = std::optional{start};
				bool closed = false;

				while (h && !seen[*h]) {
					const auto current = *h;
					seen[current] = true;
					loop.push_back(hes[current].vertex);

					h = nextBoundary(current);
					closed = h && *h == start;
				}

				if (closed && loop.size() >= 3) {
					loops.push_back(std::move(loop));
				}
			}

			return loops;
		}

		/// Edges shared by 3 or more faces
		[[nodiscard]] std::vector<EdgeKey> nonManifoldEdges() const {
			const auto& edgeToFaceMap = edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });

			std::vector<EdgeKey> out;
			for (const auto& [edge, faces] : edgeToFaceMap) {
				if (faces.size() > 2) {
					out.push_back(edge);
				}
			}
			return out;
		}

		[[nodiscard]] auto getFaceswithEdge(EdgeKey key) const {
			const auto& edgeToFaceMap = edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });
			return edgeToFaceMap.at(key.canonize());
		}

		[[nodiscard]] std::vector<FaceRef> facesAtVertex(const VertexRef v) const {
			const auto& vertexHalfedges = vertexHalfedges_.getOrCreate([this] { return calcVertexHalfedges(); });

			std::vector<FaceRef> faces;
			faces.reserve(vertexHalfedges[v].size());

			for (const auto h : vertexHalfedges[v])
				faces.push_back(halfedges_[h].face);

			return faces;
		}

		// Returns the seperate sections of a star around a given vertex
		//  For a normal interior star this is one set, since all faces can "reach" eachother
		//  A non-manifold edge, or boundary edge can break this connection between faces and result in more subsets.
		//  There is more info on this in section 4.1 of http://mesh.brown.edu/taubin/pdfs/Gueziec-etal-tvcg01.pdf
		[[nodiscard]] std::vector<std::vector<FaceRef>> vertexStarSubsets(const VertexRef v) const {
			const auto& edgeToFaces = edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });
			const auto& vertexEdges = vertexEdges_.getOrCreate([this] { return calcVertexEdges(); });

			std::unordered_map<FaceRef, std::vector<FaceRef>> adjacency;

			for (const auto& edge : vertexEdges[v]) {
				const auto& faces = edgeToFaces.at(edge);

				if (faces.size() != 2) // No boundary or non-manifold edges
					continue;

				const FaceRef a = faces[0];
				const FaceRef b = faces[1];
				adjacency[a].push_back(b);
				adjacency[b].push_back(a);
			}

			std::unordered_set<FaceRef> visited;
			std::vector<std::vector<FaceRef>> components;

			for (const FaceRef start : facesAtVertex(v)) {
				if (!visited.insert(start).second) {
					continue;
				}

				auto& component = components.emplace_back();

				std::vector stack{start};

				while (!stack.empty()) {
					const FaceRef f = stack.back();
					stack.pop_back();

					component.push_back(f);

					for (const FaceRef neighbor : adjacency[f]) {
						if (!visited.insert(neighbor).second) {
							continue;
						}
						stack.push_back(neighbor);
					}
				}
			}

			return components;
		}

	private:
		[[nodiscard]] const Halfedge& next(const HalfedgeRef h) const {
			return halfedges_[halfedges_[h].next];
		}

		[[nodiscard]] const Halfedge& prev(const HalfedgeRef h) const {
			return halfedges_[halfedges_[h].prev];
		}

		[[nodiscard]] const Halfedge& opposite(const HalfedgeRef h) const {
			return halfedges_[*halfedges_[h].opposite];
		}

		[[nodiscard]] bool has_opposite(const HalfedgeRef h) const {
			return halfedges_[h].opposite.has_value();
		}

		[[nodiscard]] EdgeToFaceMap calcEdgeFaces() const {
			EdgeToFaceMap map;
			for (const auto& he : halfedges_) {
				const VertexRef from = halfedges_[he.prev].vertex;
				const VertexRef to = he.vertex;
				EdgeKey const edge{from, to};
				map[edge.canonize()].push_back(he.face);
			}
			return map;
		}

		[[nodiscard]] std::vector<std::vector<HalfedgeRef>> calcVertexHalfedges() const {
			std::vector<std::vector<HalfedgeRef>> result(vertex_halfedge_.size());

			for (HalfedgeRef h = 0; h < halfedges_.size(); ++h)
				result[halfedges_[h].vertex].push_back(h);

			return result;
		}

		[[nodiscard]] std::vector<std::vector<EdgeKey>> calcVertexEdges() const {
			const auto& edgeToFaces = edgeToFace_.getOrCreate([this] { return calcEdgeFaces(); });

			std::vector<std::vector<EdgeKey>> result(vertex_halfedge_.size());

			for (const auto& edge : edgeToFaces | std::views::keys) {
				result[edge.a].push_back(edge);
				result[edge.b].push_back(edge);
			}

			return result;
		}
	};

} // namespace half_edge


#endif // MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
