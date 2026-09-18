//
// Created by dafitius on 17-09-2026.
//

#ifndef QTTEST_UTIL_H
#define QTTEST_UTIL_H
#include <string>
#include <vector>

namespace io_utils
{
	inline std::vector<std::string> split(std::string s, const std::string& delimiter)
	{
		std::vector<std::string> tokens;
		size_t pos = 0;
		while ((pos = s.find(delimiter)) != std::string::npos)
		{
			std::string token = s.substr(0, pos);
			tokens.push_back(token);
			s.erase(0, pos + delimiter.length());
		}
		tokens.push_back(s);

		return tokens;
	}

	// Prefer this function over std::stof since it's unaffected by locale settings.
	template <typename T>
		requires std::is_floating_point_v<T>
	inline T string_to_float(const std::string& s)
	{
		std::istringstream iss(s);
		iss.imbue(std::locale::classic());
		float v = 0.0f;
		iss >> v;
		return v;
	}

	inline auto trim(std::string_view s)
	{
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
		{
			s.remove_prefix(1);
		}
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
		{
			s.remove_suffix(1);
		}
		return std::string(s);
	};
} // namespace io_utils

#endif // QTTEST_UTIL_H