#pragma once
#include <memory>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

namespace vw {
	class Log
	{	
	public:
		static void Init();		

		inline static void SetLogLevel(spdlog::level::level_enum level) { m_logger->set_level(level); }
		inline static std::shared_ptr<spdlog::logger>& GetLogger() { return m_logger; }	
	private:
		static std::shared_ptr<spdlog::logger> m_logger;
	};
}

#define LOG_TRACE(...) ::vw::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::vw::Log::GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::vw::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::vw::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::vw::Log::GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::vw::Log::GetLogger()->critical(__VA_ARGS__)

/*
trace = SPDLOG_LEVEL_TRACE,
debug = SPDLOG_LEVEL_DEBUG,
info = SPDLOG_LEVEL_INFO,
warn = SPDLOG_LEVEL_WARN,
err = SPDLOG_LEVEL_ERROR,
critical = SPDLOG_LEVEL_CRITICAL,
off = SPDLOG_LEVEL_OFF,*/