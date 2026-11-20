//
// Created by DanioHans on 15-10-2026.
//

#include "stats_table.hpp"

#include "../io/shape_db.hpp"
#include "../io/util.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace stats
{
	namespace
	{
		constexpr std::string_view labelPrefix = "# label:";
		constexpr size_t numKeyColumns = 2;

		std::vector<std::string_view> splitFields(const std::string_view line) {
			std::vector<std::string_view> fields;
			std::ranges::copy(io_utils::split(line, ',') | std::views::transform(io_utils::trim), std::back_inserter(fields));
			return fields;
		}

		std::string rowKey(const ShapeRow& row) {
			return row.className + '/' + row.fileName;
		}
	} // namespace

	std::optional<size_t> StatsTable::columnIndex(const std::string_view name) const {
		const auto found = std::ranges::find(columns_, name);
		return found != columns_.end() ? std::make_optional(static_cast<size_t>(std::distance(columns_.begin(), found))) : std::nullopt;
	}

	void StatsTable::addRow(ShapeRow row) {
		assert(row.values.size() == columns_.size());
		rows_.push_back(std::move(row));
	}

	void StatsTable::sortRows() {
		std::ranges::sort(rows_, {}, [](const ShapeRow& row) { return std::tie(row.className, row.fileName); });
	}

	std::vector<double> StatsTable::columnValues(const size_t column) const {
		std::vector<double> values;
		values.reserve(rows_.size());
		std::ranges::transform(rows_, std::back_inserter(values), [column](const ShapeRow& row) { return row.values[column]; });
		return values;
	}

	std::vector<ShapeRef> StatsTable::shapes() const {
		std::vector<ShapeRef> shapes;
		shapes.reserve(rows_.size());
		std::ranges::transform(rows_, std::back_inserter(shapes),
							   [](const ShapeRow& row) { return ShapeRef{.className = row.className, .fileName = row.fileName}; });
		return shapes;
	}

	bool StatsTable::writeCsv(const std::filesystem::path& path) const {
		std::ofstream file(path);
		if (!file.is_open()) {
			std::cerr << "Unable to write file: " << path.string() << '\n';
			return false;
		}

		file << labelPrefix << ' ' << label_ << '\n';
		file << "class,file";
		for (const auto& column : columns_) {
			file << ',' << column;
		}
		file << '\n';

		for (const auto& row : rows_) {
			file << row.className << ',' << row.fileName;
			for (const auto value : row.values) {
				file << ',' << std::format("{}", value);
			}
			file << '\n';
		}
		return file.good();
	}

	std::optional<StatsTable> StatsTable::readCsv(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << "Unable to open file: " << path.string() << '\n';
			return std::nullopt;
		}

		std::string label = path.stem().string();
		std::string line;
		std::optional<StatsTable> table;

		for (size_t lineNumber = 1; std::getline(file, line); ++lineNumber) {
			const auto trimmed = io_utils::trim(line);
			if (trimmed.empty()) {
				continue;
			}

			if (trimmed.starts_with('#')) {
				if (trimmed.starts_with(labelPrefix)) {
					label = io_utils::trim(trimmed.substr(labelPrefix.size()));
				}
				continue;
			}

			const auto fields = splitFields(trimmed);

			if (!table) {
				if (fields.size() < numKeyColumns || fields[0] != "class" || fields[1] != "file") {
					std::cerr << "Expected a class,file,... header in: " << path.string() << '\n';
					return std::nullopt;
				}
				std::vector<std::string> columns(fields.begin() + numKeyColumns, fields.end());
				table.emplace(label, std::move(columns));
				continue;
			}

			if (fields.size() != numKeyColumns + table->columns().size()) {
				std::cerr << std::format("Expected {} fields on line {} in: {}\n", numKeyColumns + table->columns().size(), lineNumber,
										 path.string());
				return std::nullopt;
			}

			ShapeRow row{.className = std::string(fields[0]), .fileName = std::string(fields[1]), .values = {}};
			row.values.reserve(table->columns().size());
			for (const auto field : fields | std::views::drop(numKeyColumns)) {
				const auto value = io_utils::parseNum<double>(field);
				if (!value) {
					std::cerr << std::format("Invalid number '{}' on line {} in: {}\n", field, lineNumber, path.string());
					return std::nullopt;
				}
				row.values.push_back(*value);
			}
			table->addRow(std::move(row));
		}

		if (!table) {
			std::cerr << "No header found in: " << path.string() << '\n';
		}
		return table;
	}

	std::vector<std::pair<size_t, size_t>> matchRows(const StatsTable& first, const StatsTable& second) {
		std::unordered_map<std::string, size_t> secondRows;
		secondRows.reserve(second.rows().size());
		for (const auto [index, row] : second.rows() | std::views::enumerate) {
			secondRows.emplace(rowKey(row), static_cast<size_t>(index));
		}

		std::vector<std::pair<size_t, size_t>> pairs;
		for (const auto [index, row] : first.rows() | std::views::enumerate) {
			if (const auto found = secondRows.find(rowKey(row)); found != secondRows.end()) {
				pairs.emplace_back(static_cast<size_t>(index), found->second);
			}
		}
		return pairs;
	}
} // namespace stats
