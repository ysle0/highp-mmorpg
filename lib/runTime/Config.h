#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include "TomlParser.h"

namespace highp::config {

// Runtime config accessor
// Usage:
//   auto config = Config::FromFile("config.toml");
//   int port = config.Int("server.port", 8080);
//   // Or with environment variable override:
//   int port = config.Int("server.port", 8080, "SERVER_PORT");
//
class Config {
public:
	Config() = default;
	explicit Config(TomlParseResult result) : _result(std::move(result)) {}

	// Factory method - load from file
	[[nodiscard]] static std::optional<Config> FromFile(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			return std::nullopt;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		return Config(TomlParser::Parse(buffer.str()));
	}

	// Factory method - parse from string
	[[nodiscard]] static Config FromString(std::string_view content) {
		return Config(TomlParser::Parse(content));
	}

	// Reload config from file
	bool Reload(const std::filesystem::path& path) {
		auto newConfig = FromFile(path);
		if (!newConfig.has_value()) {
			return false;
		}
		_result = std::move(newConfig->_result);
		return true;
	}

	// Accessors with default values and optional environment variable override
	[[nodiscard]] std::string Str(
		std::string_view key,
		std::string_view defaultValue = {},
		const char* envVar = nullptr) const {

		if (envVar != nullptr) {
			if (auto env = GetEnv(envVar); !env.empty()) {
				return env;
			}
		}

		auto value = _result.FindString(key);
		return value.value_or(std::string(defaultValue));
	}

	[[nodiscard]] int Int(
		std::string_view key,
		int defaultValue = 0,
		const char* envVar = nullptr) const {

		if (envVar != nullptr) {
			if (auto env = GetEnv(envVar); !env.empty()) {
				if (auto parsed = StringUtils::ParseInt(env)) {
					return parsed.value();
				}
			}
		}

		auto value = _result.FindInt(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] long Long(
		std::string_view key,
		long defaultValue = 0,
		const char* envVar = nullptr) const {

		if (envVar != nullptr) {
			if (auto env = GetEnv(envVar); !env.empty()) {
				if (auto parsed = StringUtils::ParseLong(env)) {
					return parsed.value();
				}
			}
		}

		auto value = _result.FindLong(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] bool Bool(
		std::string_view key,
		bool defaultValue = false,
		const char* envVar = nullptr) const {

		if (envVar != nullptr) {
			if (auto env = GetEnv(envVar); !env.empty()) {
				if (auto parsed = StringUtils::ParseBool(env)) {
					return parsed.value();
				}
			}
		}

		auto value = _result.FindBool(key);
		return value.value_or(defaultValue);
	}

	[[nodiscard]] size_t EntryCount() const noexcept {
		return _result.Count();
	}

	[[nodiscard]] bool IsLoaded() const noexcept {
		return _result.Count() > 0;
	}

private:
	TomlParseResult _result{};

	static std::string GetEnv(const char* name) {
		if (name == nullptr) return {};

#ifdef _WIN32
		char* value = nullptr;
		size_t len = 0;
		if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
			std::string result(value);
			free(value);
			return result;
		}
		return {};
#else
		const char* value = std::getenv(name);
		return value ? std::string(value) : std::string{};
#endif
	}
};

} // namespace highp::config
