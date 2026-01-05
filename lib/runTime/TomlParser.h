#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "StringUtils.h"
#include "TomlEntry.h"

namespace highp::lib::config::runTime {

struct TomlParseResult {
	std::vector<TomlEntry> entries;

	void Add(TomlEntry entry) {
		entries.push_back(std::move(entry));
	}

	[[nodiscard]] std::optional<std::string> FindString(std::string_view fullKey) const {
		for (const auto& entry : entries) {
			if (entry.MatchesFullKey(fullKey)) {
				return StringUtils::Unquote(entry.value);
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<int> FindInt(std::string_view fullKey) const {
		for (const auto& entry : entries) {
			if (entry.MatchesFullKey(fullKey)) {
				return StringUtils::ParseInt(entry.value);
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<long> FindLong(std::string_view fullKey) const {
		for (const auto& entry : entries) {
			if (entry.MatchesFullKey(fullKey)) {
				return StringUtils::ParseLong(entry.value);
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<bool> FindBool(std::string_view fullKey) const {
		for (const auto& entry : entries) {
			if (entry.MatchesFullKey(fullKey)) {
				return StringUtils::ParseBool(entry.value);
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] size_t Count() const noexcept {
		return entries.size();
	}
};

struct TomlParser {
	static TomlParseResult Parse(std::string_view content) {
		TomlParseResult res{};
		std::string currentSection{};

		size_t lineStart = 0;
		while (lineStart < content.size()) {
			size_t lineEnd = lineStart;
			while (lineEnd < content.size() && content[lineEnd] != '\n') {
				++lineEnd;
			}

			size_t actualEnd = lineEnd;
			if (actualEnd > lineStart && content[actualEnd - 1] == '\r') {
				--actualEnd;
			}

			std::string_view line = content.substr(lineStart, actualEnd - lineStart);
			line = StringUtils::Trim(line);

			if (!line.empty() && line[0] != '#') {
				if (line[0] == '[') {
					currentSection = ParseSection(line);
				} else {
					auto entry = ParseKeyValue(line, currentSection);
					if (entry.has_value()) {
						res.Add(std::move(entry.value()));
					}
				}
			}

			lineStart = lineEnd + 1;
		}

		return res;
	}

private:
	static std::string ParseSection(std::string_view line) {
		if (line.size() < 3 || line.front() != '[') {
			return {};
		}

		auto closeBracket = StringUtils::Find(line, ']');
		if (!closeBracket.has_value()) {
			return {};
		}

		auto section = StringUtils::Trim(line.substr(1, closeBracket.value() - 1));
		return std::string(section);
	}

	static std::optional<TomlEntry> ParseKeyValue(
		std::string_view line,
		const std::string& currentSection) {

		auto eqPos = StringUtils::Find(line, '=');
		if (!eqPos.has_value()) {
			return std::nullopt;
		}

		auto key = StringUtils::Trim(line.substr(0, eqPos.value()));
		auto value = StringUtils::Trim(line.substr(eqPos.value() + 1));

		auto commentPos = StringUtils::Find(value, '#');
		if (commentPos.has_value()) {
			value = StringUtils::Trim(value.substr(0, commentPos.value()));
		}

		TomlValueType type = TomlValueType::String;
		if (value == "true" || value == "false") {
			type = TomlValueType::Boolean;
		} else if (!value.empty() && (StringUtils::IsDigit(value[0]) || value[0] == '-' || value[0] == '+')) {
			type = TomlValueType::Integer;
		}

		return TomlEntry{
			.section = currentSection,
			.key = std::string(key),
			.value = std::string(value),
			.type = type
		};
	}
};

} // namespace highp::lib::config::runTime
