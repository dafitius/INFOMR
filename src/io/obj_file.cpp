//
// Created by dafitius on 19-10-2026.
//

#include "obj_file.h"
#include "../geometry/mesh.hpp"

std::optional<Mesh> ObjFile::read(const std::filesystem::path& filePath) {
	// Read entire file into memory
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Can't open file" << filePath.string() << '\n';
		return std::nullopt;
	}

	file.seekg(0, std::ios::end);
	const auto totalSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string buffer(totalSize, '\0');
	if (!file.read(buffer.data(), totalSize)) {
		std::cerr << "Failed to read file: " << filePath.string() << '\n';
		return std::nullopt;
	}
	file.close();

	const auto [numPositions, numNormals, numFaces] = countElements(buffer);

	std::vector<std::array<float, 3>> positions;
	std::vector<std::array<float, 3>> normals;
	FaceList faces;
	positions.reserve(numPositions);
	normals.reserve(numNormals);
	faces.reserve(numFaces, numFaces * 3);

	std::vector<FaceList::Index> face; // scratch, reused for every face
	face.reserve(8);

	auto lines = std::string_view(buffer) | std::views::split('\n') | std::views::transform([](auto part) {
					 const std::string_view sv{part.data(), static_cast<size_t>(part.size())};
					 return io_utils::trim(sv);
				 });

	for (auto line : lines) {
		if (line.size() < 2 || line[0] == '#') {
			continue;
		}

		const char tag0 = line[0];
		const char tag1 = line[1];

		if (tag0 == 'v' && tag1 == ' ') {
			auto payload = io_utils::trim(line.substr(1));
			if (auto triplet = io_utils::parseTriplet<float>(payload)) {
				positions.push_back(*triplet);
			}
		} else if (tag0 == 'v' && tag1 == 'n') {
			auto payload = io_utils::trim(line.substr(2));
			if (auto triplet = io_utils::parseTriplet<float>(payload)) {
				normals.push_back(*triplet);
			}
		} else if (tag0 == 'f' && tag1 == ' ') {
			face.clear();
			bool valid = true;

			io_utils::forEachToken(line.substr(1), [&](const std::string_view tok) {
				const auto idx = parseFaceIndex(tok, positions.size());
				if (idx) {
					face.push_back(static_cast<FaceList::Index>(*idx));
				} else {
					valid = false;
				}
			});

			if (valid && face.size() >= 3) {
				faces.push_back(face);
			}
		}
	}

	auto convert = [](const auto& src) {
		std::vector<glm::vec3> dst;
		dst.reserve(src.size());
		std::ranges::transform(src, std::back_inserter(dst), [](const std::array<float, 3>& a) { return glm::vec3{a[0], a[1], a[2]}; });
		return dst;
	};

	return Mesh{convert(positions), convert(normals), std::move(faces)};
}

bool ObjFile::write(const std::filesystem::path& path, const Mesh& mesh) {
	std::ofstream file(path);
	if (!file.is_open()) {
		std::cerr << "Can't open file for writing: " << path.string() << '\n';
		return false;
	}

	file << std::setprecision(std::numeric_limits<float>::max_digits10);

	for (const auto& position : mesh.positions()) {
		file << "v " << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
	}

	for (const auto& normal : mesh.normals()) {
		file << "vn " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << '\n';
	}

	for (const auto& face : mesh.faceList().all()) {
		file << 'f';

		for (const auto index : face) {
			file << ' ' << static_cast<std::size_t>(index) + 1;
		}

		file << '\n';
	}

	if (!file) {
		std::cerr << "Failed to write file: " << path.string() << '\n';
		return false;
	}

	return true;
}

ObjFile::ElementCounts ObjFile::countElements(const std::string_view text) {
	ElementCounts counts;
	size_t pos = 0;
	while (pos + 1 < text.size()) {
		const char c0 = text[pos];
		const char c1 = text[pos + 1];
		if (c0 == 'v' && c1 == ' ') {
			++counts.positions;
		} else if (c0 == 'v' && c1 == 'n') {
			++counts.normals;
		} else if (c0 == 'f' && c1 == ' ') {
			++counts.faces;
		}

		const auto newline = text.find('\n', pos);
		if (newline == std::string_view::npos) {
			break;
		}
		pos = newline + 1;
	}
	return counts;
}

std::optional<size_t> ObjFile::parseFaceIndex(std::string_view tok, const size_t count) {
	if (const auto slash = tok.find('/'); slash != std::string_view::npos) {
		tok = tok.substr(0, slash);
	}
	tok = io_utils::trim(tok);
	if (tok.empty()) {
		return std::nullopt;
	}

	const auto idx = io_utils::parseNum<size_t>(tok);
	if (!idx || *idx == 0) {
		return std::nullopt;
	}

	const auto z = idx.value() - 1;
	if (z >= count) {
		return std::nullopt;
	}
	return z;
}