//
// Created by dafitius on 18-09-2026.
//

#ifndef QTTEST_LABEL_FILE_HPP
#define QTTEST_LABEL_FILE_HPP
#include "util.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <ranges>
#include <unordered_map>
#include <vector>

class LabelFile
{
	std::unordered_map<std::string, std::vector<size_t>> vertex_groups_;

public:
	static std::optional<LabelFile> read(const std::filesystem::path& file_path)
	{
		auto self = LabelFile();

		std::ifstream file(file_path);
		if (!file.is_open())
		{
			std::cerr << "Unable to open file!" << file_path.string() << std::endl;
			return std::nullopt;
		}

		// There is no indicator for the start of a new group,
		// so with this we just assume that a [a-Z]+ match is a new group
		auto is_group_name = [](std::string_view value)
		{
			if (value.empty())
			{
				return false;
			}

			return std::ranges::all_of(value, [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
		};

		self.vertex_groups_ = std::unordered_map<std::string, std::vector<size_t>>();
		std::string latest_group_name = "<unknown>"; // should be overwritten before indices are added
		for (std::string line; std::getline(file, line);)
		{
			if (is_group_name(io_utils::trim(line)))
			{
				latest_group_name = line;
				continue;
			}

			auto indices_view = io_utils::split(line, " ") | std::views::transform([](const auto& n) -> std::string { return io_utils::trim(n); }) |
								std::views::filter([](const auto& n)
												   { return std::any_of(n.begin(), n.end(), [](unsigned char ch) { return !std::isspace(ch); }); }) |
								std::views::transform([](const auto& n) -> size_t { return std::stoi(n); });

			auto& indices = self.vertex_groups_[latest_group_name];

			std::ranges::copy(indices_view, std::back_inserter(indices));
		}

		file.close();
		return self;
	}

	[[nodiscard]] std::unordered_map<std::string, std::vector<size_t>> vertex_groups() const
	{
		return vertex_groups_;
	}
};

#endif // QTTEST_LABEL_FILE_HPP
