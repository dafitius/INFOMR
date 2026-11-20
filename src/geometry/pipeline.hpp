//
// Created by DanioHans on 15-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_PIPELINE_HPP
#define MULTIMEDIARETRIEVAL_PIPELINE_HPP

#include "mesh.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mesh
{
	/// @brief A named mesh operation that can be used as a pipeline step.
	/// To make a new operation available everywhere (statistics page, CSV labels), add it to operations() in pipeline.cpp.
	struct Operation {
		std::string_view name;
		std::string_view description;
		Mesh (*apply)(const Mesh&);
	};

	[[nodiscard]] std::span<const Operation> operations();
	[[nodiscard]] std::optional<Operation> findOperation(std::string_view name);

	/// @brief An ordered list of operations, applied one after the other, e.g. "scale_to_unit > fill_holes"
	class Pipeline {
	public:
		static constexpr std::string_view separator = " > ";
		static constexpr std::string_view emptyLabel = "raw";

		Pipeline() = default;

		explicit Pipeline(std::vector<Operation> steps)
			: steps_(std::move(steps)) {}

		/// @brief Parses a label as produced by label(), operation names separated by '>'
		/// @return nullopt if one of the names is not a known operation
		[[nodiscard]] static std::optional<Pipeline> parse(std::string_view text);

		void add(const Operation& operation) {
			steps_.push_back(operation);
		}

		[[nodiscard]] std::span<const Operation> steps() const {
			return steps_;
		}

		[[nodiscard]] bool empty() const {
			return steps_.empty();
		}

		/// @return The operation names joined by the separator, or "raw" for an empty pipeline
		[[nodiscard]] std::string label() const;

		[[nodiscard]] Mesh apply(const Mesh& mesh) const;

	private:
		std::vector<Operation> steps_;
	};

	/// @brief The full normalization, the same steps as entering this label on the statistics page
	static constexpr std::string_view normalizationLabel =
		"orient_faces > triangulate > remove_non-manifold > orient_faces > fill_holes > remesh > center > align_axes > flip object > scale_to_unit";

	[[nodiscard]] Mesh normalized(const Mesh& mesh);
} // namespace mesh

#endif // MULTIMEDIARETRIEVAL_PIPELINE_HPP
