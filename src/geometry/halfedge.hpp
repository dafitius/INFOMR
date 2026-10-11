//
// Created by dafitius on 07-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
#define MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H

#include "face_list.hpp"


#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <vector>

namespace half_edge
{
	using HalfedgeRef = size_t;
	using FaceRef = size_t;
	using VertexRef = size_t;
	using Face = std::vector<VertexRef>;

	struct Halfedge
	{
		VertexRef vertex{};
		FaceRef face{};
		HalfedgeRef next{};
		HalfedgeRef prev{};
		std::optional<HalfedgeRef> opposite;
	};

	struct EdgeKey
	{
		VertexRef a;
		VertexRef b;

		bool operator==(const EdgeKey&) const = default;

		EdgeKey canonize() const
		{
			return {.a = std::min(a, b), .b = std::max(a, b)};
		}
	};

	struct EdgeKeyHash
	{
		std::size_t operator()(const EdgeKey& k) const noexcept
		{
			const auto low = std::min(k.a, k.b);
			const auto high = std::max(k.a, k.b);
			return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(low) << static_cast<uint32_t>(32)) | high);
		}
	};

	using EdgeToFaceMap = std::unordered_map<EdgeKey, std::vector<FaceRef>, EdgeKeyHash>;

	struct Topology
	{
	private:
		std::vector<Halfedge> halfedges_;
		std::vector<std::optional<HalfedgeRef>> vertex_halfedge_; // vertex idx -> halfedge
		std::vector<std::optional<HalfedgeRef>> face_halfedge_;	  // face idx -> halfedge
		Lazy<EdgeToFaceMap> edgeToFace_;

	public:
		[[nodiscard]] std::span<const Halfedge> halfedges() const
		{
			return std::span{halfedges_};
		}

		[[nodiscard]] auto boundaryHalfedges() const
		{
			return halfedges_ | std::views::filter([](const Halfedge& he) { return !he.opposite.has_value(); });
		}

		[[nodiscard]] std::optional<Halfedge> getHalfedgeFromVertex(const VertexRef vertex) const
		{
			const auto ref = vertex_halfedge_[vertex];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		[[nodiscard]] std::optional<Halfedge> getHalfedgeFromFace(const FaceRef face) const
		{
			const auto ref = face_halfedge_[face];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		[[nodiscard]] std::optional<HalfedgeRef> halfedgeRefFromVertex(const VertexRef vertex) const
		{
			return vertex_halfedge_[vertex];
		}

		[[nodiscard]] std::optional<HalfedgeRef> halfedgeRefFromFace(const FaceRef face) const
		{
			return face_halfedge_[face];
		}

		[[nodiscard]] bool isBoundaryHalfedge(const HalfedgeRef edge) const
		{
			return halfedges_[edge].opposite.has_value();
		}

		[[nodiscard]] bool isClosed() const
		{
			return std::ranges::none_of(halfedges_,
				[](const Halfedge& he) { return !he.opposite.has_value(); });
		}

		[[nodiscard]] std::vector<VertexRef> vertexRing(const VertexRef v) const
		{
			if (!isClosed())
			{
				std::cerr << "Tried to run vertexRing on unclosed mesh, this is unsupported!" << '\n';
				return std::vector<VertexRef>{};
			}

			std::vector<VertexRef> neighbors;

			if (v >= vertex_halfedge_.size() || !vertex_halfedge_[v].has_value())
			{
				return neighbors;
			}

			const auto start = *vertex_halfedge_[v];
			auto current = start;
			for (auto _ : std::views::iota(0u, halfedges_.size()))
			{
				neighbors.push_back(next(current).vertex);
				current = opposite(current).next;
				if (current == start)
				{
					break;
				}
			}

			return neighbors;
		}

		static Topology build(const FaceList& faces, const std::size_t numVertices)

		{
			Topology topo;
			topo.vertex_halfedge_.assign(numVertices, std::nullopt);
			topo.face_halfedge_.resize(faces.size(), std::nullopt);

			std::unordered_map<EdgeKey, HalfedgeRef, EdgeKeyHash> edgeMap;
			edgeMap.reserve(
				[&]
				{
					std::size_t total = 0;
					for (const auto& face : faces.all())
					{
						total += face.size();
					}
					return total;
				}());

			for (const auto& [f, face] : faces.all() | std::views::enumerate)
			{
				const auto faceStart = topo.halfedges_.size();
				topo.face_halfedge_[f] = faceStart;

				const std::size_t numFaces = face.size();
				topo.halfedges_.resize(topo.halfedges_.size() + numFaces);

				for (std::size_t i = 0; i < numFaces; ++i)
				{
					const HalfedgeRef edgeIndex = faceStart + i;
					const HalfedgeRef nextIndex = faceStart + ((i + 1) % numFaces);
					const HalfedgeRef prevIndex = faceStart + ((i + numFaces - 1) % numFaces);

					const auto origin = static_cast<VertexRef>(face[i]);
					const auto dest = static_cast<VertexRef>(face[(i + 1) % numFaces]);

					topo.halfedges_[edgeIndex] = Halfedge{
						.vertex = origin,
						.face = static_cast<size_t>(f),
						.next = nextIndex,
						.prev = prevIndex,
						.opposite = std::nullopt,
					};

					if (topo.vertex_halfedge_[origin] == std::nullopt)
					{
						topo.vertex_halfedge_[origin] = edgeIndex;
					}

					auto canonizeEdge = [](const VertexRef a, const VertexRef b) -> EdgeKey { return {.a = std::min(a, b), .b = std::max(a, b)}; };

					const EdgeKey key = canonizeEdge(origin, dest);
					if (auto [result, inserted] = edgeMap.emplace(key, edgeIndex); !inserted)
					{
						const HalfedgeRef other = result->second;
						topo.halfedges_[edgeIndex].opposite = other;
						topo.halfedges_[other].opposite = edgeIndex;
						edgeMap.erase(result);
					}
				}
			}

			for (HalfedgeRef h = 0; h < topo.halfedges_.size(); ++h)
			{
				const auto& he = topo.halfedges_[h];
				if (!he.opposite.has_value())
				{
					topo.vertex_halfedge_[he.vertex] = h;
				}
			}

			return topo;
		}

		[[nodiscard]] std::vector<std::vector<std::size_t>> boundaryLoops() const
		{
			const auto& hes = this->halfedges();

			auto nextBoundary = [&](const HalfedgeRef edgeIndex) -> std::optional<HalfedgeRef>
			{
				auto nextEdgeIndex = hes[edgeIndex].next;
				for (std::size_t guard = 0; hes[nextEdgeIndex].opposite.has_value() && guard < hes.size(); ++guard)
				{
					nextEdgeIndex = opposite(nextEdgeIndex).next;
				}
				return hes[nextEdgeIndex].opposite.has_value() ? std::nullopt : std::make_optional(nextEdgeIndex);
			};

			std::vector seen(hes.size(), false);
			std::vector<std::vector<std::size_t>> loops;

			for (HalfedgeRef start = 0; start < hes.size(); ++start)
			{
				if (isBoundaryHalfedge(start) || seen[start])
				{
					continue;
				}

				std::vector<std::size_t> loop;
				auto h = std::make_optional(start);
				bool closed = false;

				while (h && !seen[h.value()])
				{
					const auto current = h.value();
					seen[current] = true;
					loop.push_back(hes[current].vertex);
					h = nextBoundary(current);
					closed = h && h.value() == start;
				}

				if (closed && loop.size() >= 3)
				{
					loops.push_back(std::move(loop));
				}
			}

			return loops;
		}

		/// Edges shared by 3 or more faces
		[[nodiscard]] std::vector<EdgeKey> nonManifoldEdges()
		{
			auto edgeToFaceMap = edgeToFace_.getOrCreate([this]{return calcEdgeFaces();});

			std::vector<EdgeKey> out;
			for (const auto& [edge, faces] : edgeToFaceMap)
			{
				if (faces.size() > 2)
				{
					out.push_back(edge);
				}
			}
			return out;
		}

	private:
		[[nodiscard]] const Halfedge& next(const HalfedgeRef h) const
		{
			return halfedges_[halfedges_[h].next];
		}

		[[nodiscard]] const Halfedge& prev(const HalfedgeRef h) const
		{
			return halfedges_[halfedges_[h].next];
		}

		[[nodiscard]] const Halfedge& opposite(const HalfedgeRef h) const
		{
			return halfedges_[*halfedges_[h].opposite];
		}

		[[nodiscard]] EdgeToFaceMap calcEdgeFaces() const
		{
			EdgeToFaceMap map;
			for (const auto& he : halfedges_)
			{
				const VertexRef from = halfedges_[he.prev].vertex;
				const VertexRef to = he.vertex;
				map[EdgeKey{from, to}].push_back(he.face);
			}
			return map;
		}
	};

} // namespace half_edge


#endif // MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
