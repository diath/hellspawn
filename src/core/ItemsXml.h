/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class ItemsXml
{
	public:
		ItemsXml() = default;
		~ItemsXml() = default;

		ItemsXml(const ItemsXml &) = delete;
		ItemsXml &operator=(const ItemsXml &) = delete;
		ItemsXml(ItemsXml &&) noexcept = default;
		ItemsXml &operator=(ItemsXml &&) noexcept = default;

		bool load(const std::string &path);
		void unload();

		bool isLoaded() const { return m_loaded; }
		const std::string &filePath() const { return m_filePath; }

		std::string getItemName(uint16_t serverId) const;
		bool hasItemName(uint16_t serverId) const;

		const std::unordered_map<uint16_t, std::string> &allNames() const { return m_names; }
		size_t nameCount() const { return m_names.size(); }

	private:
		bool m_loaded = false;
		std::string m_filePath;
		std::unordered_map<uint16_t, std::string> m_names;
};
