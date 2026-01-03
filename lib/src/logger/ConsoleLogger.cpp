#include <logger/ConsoleLogger.h>

namespace highp::lib::logger {
void ConsoleLogger::Info(std::string_view msg)
{
}

void ConsoleLogger::Dbg(std::string_view msg)
{
}

void ConsoleLogger::Error(std::string_view msg)
{
}

void ConsoleLogger::Exception(std::string_view msg, std::exception const& e)
{
}
}
