#pragma once
#include <string_view>

namespace highp::config {

enum class TomlValueType {
	String,
	Integer,
	Boolean,
};

struct CompileTimeTomlEntry {
	std::string_view section;   // e.g., "server"
	std::string_view key;       // e.g., "port"
	std::string_view value;     // raw string value
	TomlValueType ioType;

	constexpr bool Matches(std::string_view sec, std::string_view k) const noexcept {
		if (section.size() != sec.size() || key.size() != k.size())
			return false;

		for (size_t i = 0; i < section.size(); ++i) {
			if (section[i] != sec[i]) return false;
		}
		for (size_t i = 0; i < key.size(); ++i) {
			if (key[i] != k[i]) return false;
		}
		return true;
	}

	// Match with "section.key" format
	constexpr bool MatchesFullKey(std::string_view fullKey) const noexcept {
		// Find the dot separator
		size_t dotPos = 0;
		bool found = false;
		for (size_t i = 0; i < fullKey.size(); ++i) {
			if (fullKey[i] == '.') {
				dotPos = i;
				found = true;
				break;
			}
		}

		if (!found) {
			// No section, match key only with empty section
			return section.empty() && Matches("", fullKey);
		}

		auto sec = fullKey.substr(0, dotPos);
		auto k = fullKey.substr(dotPos + 1);
		return Matches(sec, k);
	}
};

} // namespace highp::config
