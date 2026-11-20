//
// Created by DanioHans on 15-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_STATS_TABLE_HPP
#define MULTIMEDIARETRIEVAL_STATS_TABLE_HPP

#include "../io/shape_db.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stats
{
	struct ShapeRow {
		std::string className;
		std::string fileName;
		std::vector<double> values;
	};

	/// @brief Named numeric columns with one row per shape, e.g. the metrics of the database after a pipeline (or extracted features).
	/// Stored as CSV: an optional "# label: ..." line, a "class,file,<columns...>" header and one line per shape.
	class StatsTable {
	public:
		StatsTable() = default;

		StatsTable(std::string label, std::vector<std::string> columns)
			: label_(std::move(label))
			, columns_(std::move(columns)) {}

		[[nodiscard]] const std::string& label() const {
			return label_;
		}

		void setLabel(std::string label) {
			label_ = std::move(label);
		}

		[[nodiscard]] std::span<const std::string> columns() const {
			return columns_;
		}

		[[nodiscard]] std::span<const ShapeRow> rows() const {
			return rows_;
		}

		[[nodiscard]] std::optional<size_t> columnIndex(std::string_view name) const;

		void addRow(ShapeRow row);

		void sortRows();

		[[nodiscard]] std::vector<double> columnValues(size_t column) const;

		[[nodiscard]] std::vector<ShapeRef> shapes() const;

		bool writeCsv(const std::filesystem::path& path) const;

		[[nodiscard]] static std::optional<StatsTable> readCsv(const std::filesystem::path& path);

	private:
		std::string label_;
		std::vector<std::string> columns_;
		std::vector<ShapeRow> rows_;
	};

	[[nodiscard]] std::vector<std::pair<size_t, size_t>> matchRows(const StatsTable& first, const StatsTable& second);
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_STATS_TABLE_HPP
