#include "pch.h"
#include "Logger.h"

void Dune::Logger::Info(const char* msg)
{
	Log(LogLevel::Info, msg);
}

void Dune::Logger::Warning(const char* msg)
{
	Log(LogLevel::Warning, msg);
}

void Dune::Logger::Error(const char* msg)
{
	Log(LogLevel::Error, msg);
}

void Dune::Logger::Critical(const char* msg)
{
	Log(LogLevel::Critical, msg);
}

void Dune::Logger::Log(LogLevel level, const char* msg)
{
	switch (level)
	{
	case LogLevel::Info:
		std::cout << "[INFO] " << msg << std::endl;
			break;
	case LogLevel::Warning:
		std::cout << "[WARNING] " << msg << std::endl;
		break;
	case LogLevel::Error:
		std::cout << "[ERROR] " << msg << std::endl;
		break;
	case LogLevel::Critical:
		std::cout << "[CRITICAL] " << msg << std::endl;
		break;
	default:
		std::cout << "[UNDEFINED] " << msg << std::endl;
		break;
	}
}