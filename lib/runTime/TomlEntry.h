#pragma once

#include <string>
#include <string_view>
#include "../compileTime/TomlEntry.h"  // for TomlValueType

namespace highp::config {

struct TomlEntry {
	std::string section;
	std::string key;
	std::string value;
	TomlValueType type;

	[[nodiscard]] bool Matches(std::string_view sec, std::string_view k) const noexcept {
		return section == sec && key == k;
	}

	[[nodiscard]] bool MatchesFullKey(std::string_view fullKey) const noexcept {
		size_t dotPos = fullKey.find('.');
		if (dotPos == std::string_view::npos) {
			return section.empty() && key == fullKey;
		}

		auto sec = fullKey.substr(0, dotPos);
		auto k = fullKey.substr(dotPos + 1);
		return Matches(sec, k);
	}
};

} // namespace highp::config
