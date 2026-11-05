#pragma once
#include <filesystem>
#include <memory>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace hb {

inline std::shared_ptr<spdlog::logger> g_logger;

inline void InitLog(const std::filesystem::path& file)
{
    try
    {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
#ifdef _WIN32
        g_logger = spdlog::rotating_logger_mt("hb", file.wstring(), 1024 * 1024, 2);
#else
        g_logger = spdlog::rotating_logger_mt("hb", file.string(), 1024 * 1024, 2);
#endif
        g_logger->set_level(spdlog::level::info);
        g_logger->flush_on(spdlog::level::info);
    }
    catch (...) { g_logger.reset(); }
}

}  // namespace hb

#define HB_INFO(...) do { if (::hb::g_logger) ::hb::g_logger->info(__VA_ARGS__); } while (0)
#define HB_WARN(...) do { if (::hb::g_logger) ::hb::g_logger->warn(__VA_ARGS__); } while (0)
