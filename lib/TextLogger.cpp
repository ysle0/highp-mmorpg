#include "pch.h"
#include "TextLogger.h"

namespace highp::log {
    void TextLogger::Info(std::string_view msg) {
        std::cout << "[INFO] " << msg << std::endl;
    }

    void TextLogger::Debug(std::string_view msg) {
        std::cout << "[DEBUG] " << msg << std::endl;
    }

    void TextLogger::Warn(std::string_view msg) {
        std::cout << "[WARN] " << msg << std::endl;
    }

    void TextLogger::Error(std::string_view msg) {
        std::cout << "[ERROR] " << msg << std::endl;
    }

    void TextLogger::Exception(std::string_view msg, const std::exception& ex) {
        std::cout << "[EXCEPTION] " << msg << ": " << ex.what() << std::endl;
    }
}
