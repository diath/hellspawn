/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "core/ItemsXml.h"
#include "util/Logger.h"
#include "util/Timing.h"

#include <fmt/core.h>
#include <pugixml.hpp>

bool ItemsXml::load(const std::string &path)
{
	unload();

	int64_t startMs = Timing::currentMillis();

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(path.c_str());
	if (!result) {
		Log().error("ItemsXml: failed to parse '{}': {}", path, result.description());
		return false;
	}

	pugi::xml_node itemsNode = doc.child("items");
	if (!itemsNode) {
		Log().error("ItemsXml: no <items> root element in '{}'", path);
		return false;
	}

	for (pugi::xml_node itemNode: itemsNode.children()) {
		// Skip non-element nodes (comments, etc.)
		if (itemNode.type() != pugi::node_element) {
			continue;
		}

		std::string nameStr;
		pugi::xml_attribute nameAttr = itemNode.attribute("name");
		if (nameAttr) {
			nameStr = nameAttr.as_string();
		}

		// Handle single id="X" items
		pugi::xml_attribute idAttr = itemNode.attribute("id");
		if (idAttr) {
			uint16_t id = static_cast<uint16_t>(idAttr.as_uint(0));
			if (id > 0 && !nameStr.empty()) {
				m_names[id] = nameStr;
			}
			continue;
		}

		// Handle fromid="X" toid="Y" ranges
		pugi::xml_attribute fromIdAttr = itemNode.attribute("fromid");
		if (!fromIdAttr) {
			continue;
		}

		pugi::xml_attribute toIdAttr = itemNode.attribute("toid");
		if (!toIdAttr) {
			Log().warning("ItemsXml: fromid='{}' without toid in '{}'", fromIdAttr.value(), path);
			continue;
		}

		uint16_t fromId = static_cast<uint16_t>(fromIdAttr.as_uint(0));
		uint16_t toId = static_cast<uint16_t>(toIdAttr.as_uint(0));

		if (nameStr.empty()) {
			continue;
		}

		for (uint16_t id = fromId; id <= toId; ++id) {
			m_names[id] = nameStr;
		}
	}

	m_filePath = path;
	m_loaded = true;

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("ItemsXml: loaded '{}' — {} item names ({}ms)", path, m_names.size(), elapsedMs);
	return true;
}

void ItemsXml::unload()
{
	m_loaded = false;
	m_filePath.clear();
	m_names.clear();
}

std::string ItemsXml::getItemName(uint16_t serverId) const
{
	auto it = m_names.find(serverId);
	if (it != m_names.end()) {
		return it->second;
	}
	return {};
}

bool ItemsXml::hasItemName(uint16_t serverId) const
{
	return m_names.find(serverId) != m_names.end();
}
