#pragma once
#include <array>
#include <optional>
#include "StringUtils.h"
#include "TomlEntry.h"

namespace highp::lib::config::compileTime {

template<size_t MaxEntries>
struct TomlParseResult {
	std::array<TomlEntry, MaxEntries> entries{};
	size_t count = 0;

	constexpr bool Add(TomlEntry entry) noexcept {
		if (count >= MaxEntries) return false;
		entries[count++] = entry;
		return true;
	}

	constexpr std::optional<std::string_view> FindString(std::string_view fullKey) const noexcept {
		for (size_t i = 0; i < count; ++i) {
			if (entries[i].MatchesFullKey(fullKey)) {
				return StringUtils::Unquote(entries[i].value);
			}
		}
		return std::nullopt;
	}

	constexpr std::optional<int> FindInt(std::string_view fullKey) const noexcept {
		for (size_t i = 0; i < count; ++i) {
			if (entries[i].MatchesFullKey(fullKey)) {
				return StringUtils::ParseInt(entries[i].value);
			}
		}
		return std::nullopt;
	}

	constexpr std::optional<long> FindLong(std::string_view fullKey) const noexcept {
		for (size_t i = 0; i < count; ++i) {
			if (entries[i].MatchesFullKey(fullKey)) {
				return StringUtils::ParseLong(entries[i].value);
			}
		}
		return std::nullopt;
	}

	constexpr std::optional<bool> FindBool(std::string_view fullKey) const noexcept {
		for (size_t i = 0; i < count; ++i) {
			if (entries[i].MatchesFullKey(fullKey)) {
				return StringUtils::ParseBool(entries[i].value);
			}
		}
		return std::nullopt;
	}
};

template<size_t MaxEntries = 64>
struct TomlParser {
	static consteval TomlParseResult<MaxEntries> Parse(std::string_view content) noexcept {
		TomlParseResult<MaxEntries> res{};
		std::string_view currentSection{};

		size_t lineStart = 0;
		while (lineStart < content.size()) {
			// Find end of line
			size_t lineEnd = lineStart;
			while (lineEnd < content.size() && content[lineEnd] != '\n') {
				++lineEnd;
			}

			// Extract line (handle \r\n)
			size_t actualEnd = lineEnd;
			if (actualEnd > lineStart && content[actualEnd - 1] == '\r') {
				--actualEnd;
			}

			std::string_view line = content.substr(lineStart, actualEnd - lineStart);
			line = StringUtils::Trim(line);

			// Skip empty lines and comments
			if (!line.empty() && line[0] != '#') {
				if (line[0] == '[') {
					// Section header
					currentSection = ParseSection(line);
				} else {
					// Key-value pair
					auto entry = ParseKeyValue(line, currentSection);
					if (entry.has_value()) {
						res.Add(entry.value());
					}
				}
			}

			lineStart = lineEnd + 1;
		}

		return res;
	}

private:
	static constexpr std::string_view ParseSection(std::string_view line) noexcept {
		// [section] -> section
		if (line.size() < 3) return {};
		if (line.front() != '[') return {};

		auto closeBracket = StringUtils::Find(line, ']');
		if (!closeBracket.has_value()) return {};

		return StringUtils::Trim(line.substr(1, closeBracket.value() - 1));
	}

	static constexpr std::optional<TomlEntry> ParseKeyValue(
		std::string_view line,
		std::string_view currentSection) noexcept {
		auto eqPos = StringUtils::Find(line, '=');
		if (!eqPos.has_value()) return std::nullopt;

		std::string_view key = StringUtils::Trim(line.substr(0, eqPos.value()));
		std::string_view value = StringUtils::Trim(line.substr(eqPos.value() + 1));

		// Remove inline comments
		auto commentPos = StringUtils::Find(value, '#');
		if (commentPos.has_value()) {
			value = StringUtils::Trim(value.substr(0, commentPos.value()));
		}

		// Determine type
		TomlValueType type = TomlValueType::String;
		if (StringUtils::Equals(value, "true") || StringUtils::Equals(value, "false")) {
			type = TomlValueType::Boolean;
		} else if (!value.empty() && (StringUtils::IsDigit(value[0]) || value[0] == '-' || value[0] == '+')) {
			type = TomlValueType::Integer;
		}

		return TomlEntry{
			.section = currentSection,
			.key = key,
			.value = value,
			.type = type
		};
	}
};

}; // namespace highp::lib::config::compileTime
