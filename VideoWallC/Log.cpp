#include "Log.h"

namespace vw {
	std::shared_ptr<spdlog::logger> Log::m_logger;

	void Log::Init()
	{
		m_logger = spdlog::rotating_logger_mt("Main", "output.log", 1024 * 1024 * 20, 20);
		m_logger->flush_on(spdlog::level::debug);
		m_logger->set_pattern("[%d.%m.%Y %T.%f][%l]  %v");
		m_logger->set_level(spdlog::level::trace);
	}
}
