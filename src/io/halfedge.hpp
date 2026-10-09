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

namespace HalfEdge
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
			const auto lo = std::min(k.a, k.b);
			const auto hi = std::max(k.a, k.b);
			return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(lo) << 32) | hi);
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

		[[nodiscard]] auto boundary_halfedges() const
		{
			return halfedges_ | std::views::filter([](const Halfedge& he) { return !he.opposite.has_value(); });
		}

		[[nodiscard]] std::optional<Halfedge> get_halfedge_from_vertex(const VertexRef vertex) const
		{
			const auto ref = vertex_halfedge_[vertex];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		[[nodiscard]] std::optional<Halfedge> get_halfedge_from_face(const FaceRef face) const
		{
			const auto ref = face_halfedge_[face];
			return ref.and_then([&](const HalfedgeRef refer) { return std::make_optional<Halfedge>(halfedges_[refer]); });
		}

		static Topology build(const FaceList& faces, const std::size_t num_vertices)
		{
			Topology topo;
			topo.vertex_halfedge_.assign(num_vertices, std::nullopt);
			topo.face_halfedge_.resize(faces.size(), std::nullopt);

			std::unordered_map<EdgeKey, HalfedgeRef, EdgeKeyHash> edge_map;
			edge_map.reserve(
				[&]
				{
					std::size_t total = 0;
					for (const auto& face : faces.all())
						total += face.size();
					return total;
				}());

			for (const auto& [f, face] : faces.all() | std::views::enumerate)
			{
				const auto face_start = topo.halfedges_.size();
				topo.face_halfedge_[f] = face_start;

				const std::size_t n = face.size();
				topo.halfedges_.resize(topo.halfedges_.size() + n);

				for (std::size_t i = 0; i < n; ++i)
				{
					const HalfedgeRef h = face_start + i;
					const HalfedgeRef next = face_start + (i + 1) % n;
					const HalfedgeRef prev = face_start + (i + n - 1) % n;

					const auto origin = static_cast<VertexRef>(face[i]);
					const auto dest = static_cast<VertexRef>(face[(i + 1) % n]);

					topo.halfedges_[h] = Halfedge{
						.vertex = origin,
						.face = static_cast<size_t>(f),
						.next = next,
						.prev = prev,
						.opposite = std::nullopt,
					};

					if (topo.vertex_halfedge_[origin] == std::nullopt)
						topo.vertex_halfedge_[origin] = h;

					auto canonize_edge = [](const VertexRef a, const VertexRef b) -> EdgeKey { return {.a = std::min(a, b), .b = std::max(a, b)}; };

					const EdgeKey key = canonize_edge(origin, dest);
					if (auto [it, inserted] = edge_map.emplace(key, h); !inserted)
					{
						const HalfedgeRef other = it->second;
						topo.halfedges_[h].opposite = other;
						topo.halfedges_[other].opposite = h;
						edge_map.erase(it);
					}
				}
			}

			for (HalfedgeRef h = 0; h < topo.halfedges_.size(); ++h)
			{
				const auto& he = topo.halfedges_[h];
				if (!he.opposite.has_value())
					topo.vertex_halfedge_[he.vertex] = h;
			}

			return topo;
		}
	};

} // namespace HalfEdge


#endif // MULTIMEDIARETRIEVAL_HALFEDGETOPOLOGY_H
