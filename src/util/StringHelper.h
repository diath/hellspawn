/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace StringHelper {

inline std::string toLower(std::string_view str)
{
	std::string result(str);
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

inline std::string toUpper(std::string_view str)
{
	std::string result(str);
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return result;
}

inline std::string trim(std::string_view str)
{
	auto start = str.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos) {
		return {};
	}
	auto end = str.find_last_not_of(" \t\r\n");
	return std::string(str.substr(start, end - start + 1));
}

inline bool startsWith(std::string_view str, std::string_view prefix)
{
	if (prefix.size() > str.size()) {
		return false;
	}
	return str.substr(0, prefix.size()) == prefix;
}

inline bool endsWith(std::string_view str, std::string_view suffix)
{
	if (suffix.size() > str.size()) {
		return false;
	}
	return str.substr(str.size() - suffix.size()) == suffix;
}

inline bool containsIgnoreCase(std::string_view haystack, std::string_view needle)
{
	if (needle.empty()) {
		return true;
	}
	if (needle.size() > haystack.size()) {
		return false;
	}

	std::string lowerHaystack = toLower(haystack);
	std::string lowerNeedle = toLower(needle);
	return lowerHaystack.find(lowerNeedle) != std::string::npos;
}

inline std::string formatFileSize(uint64_t bytes)
{
	if (bytes < 1024) {
		return std::to_string(bytes) + " B";
	} else if (bytes < 1024 * 1024) {
		double kb = static_cast<double>(bytes) / 1024.0;
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.1f KB", kb);
		return buf;
	} else if (bytes < 1024ULL * 1024 * 1024) {
		double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
		return buf;
	} else {
		double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.2f GB", gb);
		return buf;
	}
}

} // namespace StringHelper
