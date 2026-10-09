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

	struct Topology
	{
	private:
		std::vector<Halfedge> halfedges_;
		std::vector<std::optional<HalfedgeRef>> vertex_halfedge_; // vertex idx -> halfedge
		std::vector<std::optional<HalfedgeRef>> face_halfedge_;	  // face idx -> halfedge

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
	};

} // namespace half_edge


#endif // MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
