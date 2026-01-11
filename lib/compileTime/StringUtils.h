#pragma once

#include <optional>
#include <string_view>

namespace highp::config {

struct CompileTimeStringUtils {
	static constexpr bool IsWhitespace(char c) noexcept {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	static constexpr bool IsDigit(char c) noexcept {
		return c >= '0' && c <= '9';
	}

	static constexpr std::string_view TrimLeft(std::string_view str) noexcept {
		size_t start = 0;
		while (start < str.size() && IsWhitespace(str[start])) {
			++start;
		}
		return str.substr(start);
	}

	static constexpr std::string_view TrimRight(std::string_view str) noexcept {
		size_t end = str.size();
		while (end > 0 && IsWhitespace(str[end - 1])) {
			--end;
		}
		return str.substr(0, end);
	}

	static constexpr std::string_view Trim(std::string_view str) noexcept {
		return TrimRight(TrimLeft(str));
	}

	static constexpr std::optional<size_t> Find(std::string_view str, char c) noexcept {
		for (size_t i = 0; i < str.size(); ++i) {
			if (str[i] == c) {
				return i;
			}
		}
		return std::nullopt;
	}

	static constexpr std::optional<size_t> FindFirst(std::string_view str, std::string_view chars) noexcept {
		for (size_t i = 0; i < str.size(); ++i) {
			for (char c : chars) {
				if (str[i] == c) {
					return i;
				}
			}
		}
		return std::nullopt;
	}

	static constexpr bool StartsWith(std::string_view str, std::string_view prefix) noexcept {
		if (prefix.size() > str.size()) return false;
		for (size_t i = 0; i < prefix.size(); ++i) {
			if (str[i] != prefix[i]) return false;
		}
		return true;
	}

	static constexpr bool Equals(std::string_view a, std::string_view b) noexcept {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i) {
			if (a[i] != b[i]) return false;
		}
		return true;
	}

	static constexpr std::optional<int> ParseInt(std::string_view str) noexcept {
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

	static constexpr std::optional<long> ParseLong(std::string_view str) noexcept {
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

	static constexpr std::optional<bool> ParseBool(std::string_view str) noexcept {
		str = Trim(str);
		if (Equals(str, "true")) return true;
		if (Equals(str, "false")) return false;
		return std::nullopt;
	}

	// Remove quotes from string value
	static constexpr std::string_view Unquote(std::string_view str) noexcept {
		str = Trim(str);
		if (str.size() >= 2) {
			if ((str.front() == '"' && str.back() == '"') ||
				(str.front() == '\'' && str.back() == '\'')) {
				return str.substr(1, str.size() - 2);
			}
		}
		return str;
	}
};

} // namespace highp::config
