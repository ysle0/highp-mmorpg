#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace highp::config {

struct StringUtils {
	static bool IsWhitespace(char c) noexcept {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	static bool IsDigit(char c) noexcept {
		return c >= '0' && c <= '9';
	}

	static std::string_view TrimLeft(std::string_view str) noexcept {
		size_t start = 0;
		while (start < str.size() && IsWhitespace(str[start])) {
			++start;
		}
		return str.substr(start);
	}

	static std::string_view TrimRight(std::string_view str) noexcept {
		size_t end = str.size();
		while (end > 0 && IsWhitespace(str[end - 1])) {
			--end;
		}
		return str.substr(0, end);
	}

	static std::string_view Trim(std::string_view str) noexcept {
		return TrimRight(TrimLeft(str));
	}

	static std::optional<size_t> Find(std::string_view str, char c) noexcept {
		size_t pos = str.find(c);
		if (pos == std::string_view::npos) {
			return std::nullopt;
		}
		return pos;
	}

	static bool StartsWith(std::string_view str, std::string_view prefix) noexcept {
		if (prefix.size() > str.size()) return false;
		return str.substr(0, prefix.size()) == prefix;
	}

	static std::optional<int> ParseInt(std::string_view str) noexcept {
		str = Trim(str);
		if (str.empty()) return std::nullopt;

		bool negative = false;
		size_t start = 0;

		if (str[0] == '-') {
			negative = true;
			start = 1;
		} else if (str[0] == '+') {
			start = 1;
		}

		if (start >= str.size()) return std::nullopt;

		int result = 0;
		for (size_t i = start; i < str.size(); ++i) {
			if (!IsDigit(str[i])) return std::nullopt;
			result = result * 10 + (str[i] - '0');
		}

		return negative ? -result : result;
	}

	static std::optional<long> ParseLong(std::string_view str) noexcept {
		str = Trim(str);
		if (str.empty()) return std::nullopt;

		bool negative = false;
		size_t start = 0;

		if (str[0] == '-') {
			negative = true;
			start = 1;
		} else if (str[0] == '+') {
			start = 1;
		}

		if (start >= str.size()) return std::nullopt;

		long result = 0;
		for (size_t i = start; i < str.size(); ++i) {
			if (!IsDigit(str[i])) return std::nullopt;
			result = result * 10 + (str[i] - '0');
		}

		return negative ? -result : result;
	}

	static std::optional<bool> ParseBool(std::string_view str) noexcept {
		str = Trim(str);
		if (str == "true") return true;
		if (str == "false") return false;
		return std::nullopt;
	}

	static std::string Unquote(std::string_view str) {
		str = Trim(str);
		if (str.size() >= 2) {
			if ((str.front() == '"' && str.back() == '"') ||
				(str.front() == '\'' && str.back() == '\'')) {
				return std::string(str.substr(1, str.size() - 2));
			}
		}
		return std::string(str);
	}
};

} // namespace highp::config
