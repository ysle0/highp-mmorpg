#pragma once
#include "TomlParser.h"

namespace highp::config {

// Compile-time config accessor
// Usage:
//   constexpr auto config = CompileTimeConfig<>::From(TOML_CONTENT);
//   constexpr int port = config.Int("server.port", 8080);
//
template<size_t MaxEntries = 64>
class CompileTimeConfig {
public:
	using ParseResult = CompileTimeTomlParseResult<MaxEntries>;

	constexpr CompileTimeConfig() = default;
	constexpr explicit CompileTimeConfig(ParseResult result) : _result(result) {}

	// Factory method - parse TOML content at compile time
	static consteval CompileTimeConfig From(std::string_view tomlContent) noexcept {
		return CompileTimeConfig(CompileTimeTomlParser<MaxEntries>::Parse(tomlContent));
	}

	// Accessors with default values
	[[nodiscard]] constexpr std::string_view Str(
		std::string_view key,
		std::string_view defaultValue = {}) const noexcept {
		auto value = _result.FindString(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] constexpr int Int(
		std::string_view key,
		int defaultValue = 0) const noexcept {
		auto value = _result.FindInt(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] constexpr long Long(
		std::string_view key,
		long defaultValue = 0) const noexcept {
		auto value = _result.FindLong(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] constexpr bool Bool(
		std::string_view key,
		bool defaultValue = false) const noexcept {
		auto value = _result.FindBool(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] constexpr size_t EntryCount() const noexcept {
		return _result.count;
	}

private:
	ParseResult _result{};
};

// Convenience macro for defining config from embedded TOML
#define DEFINE_CONSTEXPR_CONFIG(name, tomlContent) \
	inline constexpr auto name = ::highp::config::CompileTimeConfig<>::From(tomlContent)

} // namespace highp::config
