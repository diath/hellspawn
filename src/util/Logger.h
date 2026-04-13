/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <iostream>
#include <mutex>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

enum class LogLevel : uint8_t
{
	Debug,
	Info,
	Warning,
	Error
};

class Logger
{
	public:
		static Logger &instance()
		{
			static Logger logger;
			return logger;
		}

		void setMinLevel(LogLevel level) { m_minLevel = level; }
		LogLevel minLevel() const { return m_minLevel; }

		template <typename... Args>
		void debug(fmt::format_string<Args...> fmtStr, Args &&...args)
		{
			log(LogLevel::Debug, fmt::format(fmtStr, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void info(fmt::format_string<Args...> fmtStr, Args &&...args)
		{
			log(LogLevel::Info, fmt::format(fmtStr, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void warning(fmt::format_string<Args...> fmtStr, Args &&...args)
		{
			log(LogLevel::Warning, fmt::format(fmtStr, std::forward<Args>(args)...));
		}

		template <typename... Args>
		void error(fmt::format_string<Args...> fmtStr, Args &&...args)
		{
			log(LogLevel::Error, fmt::format(fmtStr, std::forward<Args>(args)...));
		}

		void log(LogLevel level, const std::string &message)
		{
			if (level < m_minLevel) {
				return;
			}

			std::lock_guard<std::mutex> lock(m_mutex);
			const char *prefix = levelPrefix(level);
			auto &stream = (level >= LogLevel::Warning) ? std::cerr : std::cout;
			stream << prefix << message << "\n";
			stream.flush();
		}

	private:
		Logger() = default;

		static const char *levelPrefix(LogLevel level)
		{
			switch (level) {
				case LogLevel::Debug:
					return "[DEBUG] ";
				case LogLevel::Info:
					return "[INFO] ";
				case LogLevel::Warning:
					return "[WARNING] ";
				case LogLevel::Error:
					return "[ERROR] ";
			}
			return "[UNKNOWN] ";
		}

		LogLevel m_minLevel = LogLevel::Info;
		std::mutex m_mutex;
};

inline Logger &Log()
{
	return Logger::instance();
}
