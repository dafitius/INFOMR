//
// Created by dafitius on 17-09-2026.
//

#ifndef MR_UTIL_H
#define MR_UTIL_H
#include <charconv>
#include <functional>
#include <qtransform.h>

namespace io_utils
{
	inline auto split(std::string_view s, char delimiter)
	{
		return s | std::views::split(delimiter) |
			   std::views::transform([](auto part) -> std::string_view { return {part.data(), static_cast<std::size_t>(part.size())}; });
	}

	template <typename T>
	static std::optional<T> parseNum(std::string_view sv)
	{
		T out{};
		auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
		if (ec != std::errc{} || p != sv.data() + sv.size())
		{
			return std::nullopt;
		}
		return out;
	}

	static std::string_view trim(std::string_view s)
	{
		while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.front())) != 0))
		{
			s.remove_prefix(1);
		}
		while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.back())) != 0))
		{
			s.remove_suffix(1);
		}
		return s;
	}

	template <typename Fn>
	static void forEachToken(std::string_view s, Fn&& fn)
	{
		size_t i = 0;
		while (i < s.size())
		{
			while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
			{
				++i;
			}
			if (i >= s.size())
			{
				break;
			}
			const size_t b = i;
			while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i])))
			{
				++i;
			}
			fn(s.substr(b, i - b));
		}
	}

	template <typename T>
	static std::optional<std::array<T, 3>> parseTriplet(std::string_view s)
	{
		std::array<T, 3> out{};
		size_t i = 0;

		forEachToken(
			s,
			[&](const std::string_view tok)
			{
				auto v = parseNum<T>(tok);
				out[i++] = *v;
			});

		if (i != 3)
		{
			return std::nullopt;
		}
		return out;
	}

} // namespace io_utils

#endif // MR_UTIL_H