#pragma once

#include <exception>
#include <format>
#include "ILogger.h"
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// TODO: #1 logger options 추가.
// TODO: #2 logger port (console, file, OTEL ...) 추가
// TODO: #3 logger format (text, json, ...) 추가

namespace highp::log {
    /// <summary>
    /// 로깅 기능을 제공하는 래퍼 클래스.
    /// ILogger 구현체에 위임하여 다양한 로깅 백엔드를 지원한다.
    /// std::format 기반 포맷팅을 지원하며, Info/Debug/Warn/Error 레벨을 제공한다.
    /// </summary>
    class Logger {
        /// <summary>실제 로깅을 수행하는 구현체</summary>
        std::shared_ptr<ILogger> _impl;
        std::string _prefix;

        [[nodiscard]] std::string CombinePrefix(std::string_view prefix) const {
            if (prefix.empty()) {
                return _prefix;
            }

            std::string combined;
            combined.reserve(_prefix.size() + prefix.size());
            combined += _prefix;
            combined += prefix;
            return combined;
        }

        [[nodiscard]] std::string BuildMessage(std::string_view msg) const {
            if (_prefix.empty()) {
                return std::string(msg);
            }

            std::string prefixed;
            prefixed.reserve(_prefix.size() + msg.size());
            prefixed += _prefix;
            prefixed += msg;
            return prefixed;
        }

    public:
        /// <summary>
        /// Logger 생성자.
        /// </summary>
        /// <param name="impl">로깅 구현체 (소유권 이전)</param>
        explicit Logger(std::shared_ptr<ILogger> impl, std::string prefix = {})
            : _impl(std::move(impl))
              , _prefix(std::move(prefix)) {
        }

        explicit Logger(std::unique_ptr<ILogger> impl, std::string prefix = {})
            : _impl(std::move(impl))
              , _prefix(std::move(prefix)) {
        }

        /// <summary>
        /// 기본 설정으로 Logger를 생성한다.
        /// </summary>
        /// <typeparam name="Impl">ILogger 구현체 타입 (예: TextLogger)</typeparam>
        /// <returns>생성된 Logger의 shared_ptr</returns>
        template <typename Impl>
        static std::shared_ptr<Logger> Default() {
            return std::make_shared<Logger>(std::make_shared<Impl>());
        }

        /// <summary>
        /// 인자를 전달하여 Logger를 생성한다.
        /// </summary>
        /// <typeparam name="Impl">ILogger 구현체 타입</typeparam>
        /// <typeparam name="Args">구현체 생성자 인자 타입들</typeparam>
        /// <param name="args">구현체 생성자에 전달할 인자들</param>
        /// <returns>생성된 Logger의 shared_ptr</returns>
        template <typename Impl, typename... Args>
        static std::shared_ptr<Logger> DefaultWithArgs(Args&&... args) {
            auto impl = std::make_shared<Impl>(std::forward<Args>(args)...);
            return std::make_shared<Logger>(std::move(impl));
        }

        [[nodiscard]] std::shared_ptr<Logger> WithPrefix(std::string_view prefix) const {
            return std::make_shared<Logger>(_impl, CombinePrefix(prefix));
        }

        /// <summary>Info 레벨 로그 출력 (단순 문자열)</summary>
        void Info(std::string_view msg) {
            if (_prefix.empty()) {
                _impl->Info(msg);
                return;
            }

            auto prefixed = BuildMessage(msg);
            _impl->Info(prefixed);
        }

        /// <summary>Debug 레벨 로그 출력 (단순 문자열)</summary>
        void Debug(std::string_view msg) {
            if (_prefix.empty()) {
                _impl->Debug(msg);
                return;
            }

            auto prefixed = BuildMessage(msg);
            _impl->Debug(prefixed);
        }

        /// <summary>Warn 레벨 로그 출력 (단순 문자열)</summary>
        void Warn(std::string_view msg) {
            if (_prefix.empty()) {
                _impl->Warn(msg);
                return;
            }

            auto prefixed = BuildMessage(msg);
            _impl->Warn(prefixed);
        }

        /// <summary>Error 레벨 로그 출력 (단순 문자열)</summary>
        void Error(std::string_view msg) {
            if (_prefix.empty()) {
                _impl->Error(msg);
                return;
            }

            auto prefixed = BuildMessage(msg);
            _impl->Error(prefixed);
        }

        /// <summary>예외 정보와 함께 로그 출력</summary>
        void Exception(std::string_view msg, const std::exception& ex) {
            if (_prefix.empty()) {
                _impl->Exception(msg, ex);
                return;
            }

            auto prefixed = BuildMessage(msg);
            _impl->Exception(prefixed, ex);
        }

        /// <summary>Info 레벨 로그 출력 (std::format 지원)</summary>
        template <class... Args>
        void Info(std::format_string<Args...> fmt, Args&&... args) {
            Info(std::vformat(fmt.get(), std::make_format_args(args...)));
        }

        /// <summary>Debug 레벨 로그 출력 (std::format 지원)</summary>
        template <class... Args>
        void Debug(std::format_string<Args...> fmt, Args&&... args) {
            Debug(std::vformat(fmt.get(), std::make_format_args(args...)));
        }

        /// <summary>Warn 레벨 로그 출력 (std::format 지원)</summary>
        template <class... Args>
        void Warn(std::format_string<Args...> fmt, Args&&... args) {
            Warn(std::vformat(fmt.get(), std::make_format_args(args...)));
        }

        /// <summary>Error 레벨 로그 출력 (std::format 지원)</summary>
        template <class... Args>
        void Error(std::format_string<Args...> fmt, Args&&... args) {
            Error(std::vformat(fmt.get(), std::make_format_args(args...)));
        }
    };
}
