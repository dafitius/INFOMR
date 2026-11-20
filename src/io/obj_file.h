//
// Created by jaelk on 25-09-2026.
//

#ifndef MR_OBJ_READER2_H
#define MR_OBJ_READER2_H

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "../geometry/face_list.hpp"
#include "glm/vec3.hpp"
#include "util.hpp"


class Mesh;

class ObjFile {
public:
	static std::optional<Mesh> read(const std::filesystem::path& filePath);
	static bool write(const std::filesystem::path& path, const Mesh& mesh);

private:
	struct ElementCounts {
		size_t positions = 0;
		size_t normals = 0;
		size_t faces = 0;
	};

	static ElementCounts countElements(std::string_view text);
	static std::optional<size_t> parseFaceIndex(std::string_view tok, size_t count);
};

#endif // MR_OBJ_READER2_H