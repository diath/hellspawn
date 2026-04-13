/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "core/OtbFile.h"
#include "util/BinaryReader.h"
#include "util/BinaryWriter.h"
#include "util/Logger.h"
#include "util/Timing.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include <fmt/core.h>

bool OtbFile::load(const std::string &path)
{
	unload();

	int64_t startMs = Timing::currentMillis();

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		Log().error("OtbFile: failed to open '{}'", path);
		return false;
	}

	auto fileSize = file.tellg();
	if (fileSize < 8) {
		Log().error("OtbFile: file '{}' is too small ({} bytes)", path, static_cast<size_t>(fileSize));
		return false;
	}

	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
	if (!file.read(reinterpret_cast<char *>(fileData.data()), fileSize)) {
		Log().error("OtbFile: failed to read '{}'", path);
		return false;
	}
	file.close();

	// First 4 bytes: identifier (must be 0x00000000 or "OTBI")
	uint32_t identifier = 0;
	std::memcpy(&identifier, fileData.data(), 4);
	if (identifier != 0) {
		// Check for "OTBI" magic — but the server FileLoader actually accepts 0x00000000
		// or a match against the accept_identifier. We'll accept both.
		char ident[4];
		std::memcpy(ident, fileData.data(), 4);
		if (std::memcmp(ident, "OTBI", 4) != 0) {
			Log().error("OtbFile: invalid identifier in '{}' (expected 0x00000000 or 'OTBI')", path);
			return false;
		}
	}

	// After the 4-byte identifier, the binary tree begins.
	// The format uses special bytes:
	//   0xFE = NODE_START
	//   0xFF = NODE_END
	//   0xFD = ESCAPE_CHAR (next byte is literal data)
	//
	// A node looks like: NODE_START <type_byte> <props...> [child nodes...] NODE_END
	//
	// The server's FileLoader parses this into a tree of NodeStruct with props.

	size_t pos = 4;

	if (pos >= fileData.size() || fileData[pos] != OTB_NODE_START) {
		Log().error("OtbFile: expected NODE_START at position 4 in '{}'", path);
		return false;
	}
	++pos; // skip NODE_START

	OtbNode root;
	if (!parseNodes(fileData, pos, root)) {
		Log().error("OtbFile: failed to parse binary tree from '{}'", path);
		return false;
	}

	if (!parseRootNode(root)) {
		Log().error("OtbFile: failed to parse root node from '{}'", path);
		return false;
	}

	for (const auto &child: root.children) {
		if (!parseItemNode(child)) {
			Log().warning("OtbFile: failed to parse an item node, skipping");
		}
	}

	m_filePath = path;
	m_loaded = true;

	rebuildLookupTables();

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("OtbFile: loaded '{}' — version={}.{}.{}, items={} ({}ms)", path, m_versionInfo.majorVersion, m_versionInfo.minorVersion, m_versionInfo.buildNumber, m_items.size(), elapsedMs);
	return true;
}

bool OtbFile::parseNodes(const std::vector<uint8_t> &fileData, size_t &pos, OtbNode &node) const
{
	// Current position should be right after the NODE_START byte.
	// First byte is the node type.
	if (pos >= fileData.size()) {
		return false;
	}

	node.type = fileData[pos++];

	// Read properties and child nodes until NODE_END
	while (pos < fileData.size()) {
		uint8_t byte = fileData[pos++];

		switch (byte) {
			case OTB_NODE_START: {
				// Start of a child node
				OtbNode child;
				if (!parseNodes(fileData, pos, child)) {
					return false;
				}
				node.children.push_back(std::move(child));
				break;
			}
			case OTB_NODE_END: {
				// End of this node
				return true;
			}
			case OTB_ESCAPE_CHAR: {
				// Next byte is escaped literal data
				if (pos >= fileData.size()) {
					return false;
				}
				node.props.push_back(fileData[pos++]);
				break;
			}
			default: {
				// Regular data byte
				node.props.push_back(byte);
				break;
			}
		}
	}

	// Reached end of file without NODE_END
	return false;
}

bool OtbFile::parseRootNode(const OtbNode &root)
{
	// Root node type should be 0
	if (root.props.size() < 4) {
		Log().error("OtbFile: root node props too small ({})", root.props.size());
		return false;
	}

	BinaryReader reader(root.props);

	// 4 bytes flags
	uint32_t flags = reader.getU32();
	(void)flags;

	// Check for version attribute
	if (reader.canRead(1)) {
		uint8_t attr = reader.getU8();
		if (attr == OtbRootAttrVersion) {
			if (!reader.canRead(2)) {
				Log().error("OtbFile: cannot read version attr length");
				return false;
			}
			uint16_t dataLen = reader.getU16();

			// VERSIONINFO is 4+4+4+128 = 140 bytes
			if (dataLen != 140) {
				Log().error("OtbFile: unexpected version info size {} (expected 140)", dataLen);
				return false;
			}

			if (!reader.canRead(140)) {
				Log().error("OtbFile: root node too small for version info");
				return false;
			}

			m_versionInfo.majorVersion = reader.getU32();
			m_versionInfo.minorVersion = reader.getU32();
			m_versionInfo.buildNumber = reader.getU32();

			// 128 bytes CSDVersion / description string
			std::vector<uint8_t> descBytes = reader.getBytes(128);
			// Find null terminator
			size_t descLen = 0;
			for (size_t i = 0; i < 128; ++i) {
				if (descBytes[i] == 0) {
					descLen = i;
					break;
				}
				descLen = i + 1;
			}
			m_versionInfo.description = std::string(reinterpret_cast<const char *>(descBytes.data()), descLen);
		}
	}

	return true;
}

bool OtbFile::parseItemNode(const OtbNode &node)
{
	if (node.props.size() < 4) {
		return false;
	}

	BinaryReader reader(node.props);

	// The node type byte is the item group (already in node.type)
	OtbItemType item;
	item.group = node.type;

	// 4 bytes flags
	item.flags = reader.getU32();

	// Read attributes
	while (reader.canRead(1)) {
		uint8_t attr = reader.getU8();
		if (attr == 0 || attr == 0xFF) {
			break;
		}

		if (!reader.canRead(2)) {
			return false;
		}
		uint16_t dataLen = reader.getU16();

		switch (attr) {
			case OtbAttrServerId: {
				if (dataLen != 2 || !reader.canRead(2)) {
					return false;
				}
				item.serverId = reader.getU16();
				if (item.serverId > 50000 && item.serverId < 50100) {
					item.serverId -= 50000;
				}
				break;
			}
			case OtbAttrClientId: {
				if (dataLen != 2 || !reader.canRead(2)) {
					return false;
				}
				item.clientId = reader.getU16();
				break;
			}
			case OtbAttrSpeed: {
				if (dataLen != 2 || !reader.canRead(2)) {
					return false;
				}
				item.speed = reader.getU16();
				break;
			}
			case OtbAttrLight2: {
				if (dataLen != 4 || !reader.canRead(4)) {
					return false;
				}
				uint16_t lightLevel = reader.getU16();
				uint16_t lightColor = reader.getU16();
				item.lightLevel = static_cast<uint8_t>(lightLevel);
				item.lightColor = static_cast<uint8_t>(lightColor);
				break;
			}
			case OtbAttrTopOrder: {
				if (dataLen != 1 || !reader.canRead(1)) {
					return false;
				}
				item.alwaysOnTopOrder = reader.getU8();
				break;
			}
			case OtbAttrWareId: {
				if (dataLen != 2 || !reader.canRead(2)) {
					return false;
				}
				item.wareId = reader.getU16();
				break;
			}
			default: {
				// Skip unknown attributes
				if (!reader.trySkip(dataLen)) {
					return false;
				}
				break;
			}
		}
	}

	m_items.push_back(std::move(item));
	return true;
}

bool OtbFile::save(const std::string &path) const
{
	if (!m_loaded) {
		Log().error("OtbFile: cannot save — no data loaded");
		return false;
	}

	int64_t startMs = Timing::currentMillis();

	std::vector<uint8_t> outData;
	outData.reserve(m_items.size() * 64 + 256);

	// Write 4-byte identifier (all zeros)
	outData.push_back(0x00);
	outData.push_back(0x00);
	outData.push_back(0x00);
	outData.push_back(0x00);

	// Build tree and serialize
	OtbNode root = buildRootNode();
	for (const auto &item: m_items) {
		root.children.push_back(buildItemNode(item));
	}

	serializeNode(root, outData);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		Log().error("OtbFile: failed to open '{}' for writing", path);
		return false;
	}

	file.write(reinterpret_cast<const char *>(outData.data()), static_cast<std::streamsize>(outData.size()));
	file.flush();
	file.close();

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("OtbFile: saved '{}' — items={} ({}ms)", path, m_items.size(), elapsedMs);
	return true;
}

void OtbFile::serializeNode(const OtbNode &node, std::vector<uint8_t> &outData) const
{
	outData.push_back(OTB_NODE_START);

	// Write node type (escaped)
	uint8_t typeByte = node.type;
	if (typeByte == OTB_NODE_START || typeByte == OTB_NODE_END || typeByte == OTB_ESCAPE_CHAR) {
		outData.push_back(OTB_ESCAPE_CHAR);
	}
	outData.push_back(typeByte);

	// Write properties (escaped)
	writeEscaped(outData, node.props.data(), node.props.size());

	// Write children
	for (const auto &child: node.children) {
		serializeNode(child, outData);
	}

	outData.push_back(OTB_NODE_END);
}

void OtbFile::writeEscaped(std::vector<uint8_t> &outData, const uint8_t *src, size_t len) const
{
	for (size_t i = 0; i < len; ++i) {
		if (src[i] == OTB_NODE_START || src[i] == OTB_NODE_END || src[i] == OTB_ESCAPE_CHAR) {
			outData.push_back(OTB_ESCAPE_CHAR);
		}
		outData.push_back(src[i]);
	}
}

OtbFile::OtbNode OtbFile::buildRootNode() const
{
	OtbNode root;
	root.type = 0;

	BinaryWriter writer;

	// Flags
	writer.addU32(0);

	// Version attribute
	writer.addU8(OtbRootAttrVersion);

	// Data length = sizeof(VERSIONINFO) = 140
	writer.addU16(140);

	writer.addU32(m_versionInfo.majorVersion);
	writer.addU32(m_versionInfo.minorVersion);
	writer.addU32(m_versionInfo.buildNumber);

	// CSDVersion: 128 bytes, null-padded description
	uint8_t descBuf[128] = {};
	size_t copyLen = std::min(m_versionInfo.description.size(), size_t {127});
	if (copyLen > 0) {
		std::memcpy(descBuf, m_versionInfo.description.data(), copyLen);
	}
	writer.addBytes(descBuf, 128);

	root.props = writer.takeBuffer();
	return root;
}

OtbFile::OtbNode OtbFile::buildItemNode(const OtbItemType &item) const
{
	OtbNode node;
	node.type = item.group;

	BinaryWriter writer;

	// Flags
	writer.addU32(item.flags);

	// Server ID attribute
	writer.addU8(OtbAttrServerId);
	writer.addU16(2); // data length
	writer.addU16(item.serverId);

	// Client ID attribute
	writer.addU8(OtbAttrClientId);
	writer.addU16(2);
	writer.addU16(item.clientId);

	// Speed (if non-zero)
	if (item.speed != 0) {
		writer.addU8(OtbAttrSpeed);
		writer.addU16(2);
		writer.addU16(item.speed);
	}

	// Light (if non-zero)
	if (item.lightLevel != 0 || item.lightColor != 0) {
		writer.addU8(OtbAttrLight2);
		writer.addU16(4);
		writer.addU16(static_cast<uint16_t>(item.lightLevel));
		writer.addU16(static_cast<uint16_t>(item.lightColor));
	}

	// Top order (if non-zero)
	if (item.alwaysOnTopOrder != 0) {
		writer.addU8(OtbAttrTopOrder);
		writer.addU16(1);
		writer.addU8(item.alwaysOnTopOrder);
	}

	// Ware ID (if non-zero)
	if (item.wareId != 0) {
		writer.addU8(OtbAttrWareId);
		writer.addU16(2);
		writer.addU16(item.wareId);
	}

	node.props = writer.takeBuffer();
	return node;
}

void OtbFile::unload()
{
	m_loaded = false;
	m_filePath.clear();
	m_versionInfo = {};
	m_items.clear();
	m_serverIdIndex.clear();
	m_clientIdIndex.clear();
	m_lookupDirty = false; // empty and consistent — no rebuild needed
}

const OtbItemType *OtbFile::getItemByServerId(uint16_t serverId) const
{
	ensureLookupTables();
	auto it = m_serverIdIndex.find(serverId);
	if (it != m_serverIdIndex.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

OtbItemType *OtbFile::getItemByServerIdMut(uint16_t serverId)
{
	ensureLookupTables();
	auto it = m_serverIdIndex.find(serverId);
	if (it != m_serverIdIndex.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

const OtbItemType *OtbFile::getItemByClientId(uint16_t clientId) const
{
	ensureLookupTables();
	auto it = m_clientIdIndex.find(clientId);
	if (it != m_clientIdIndex.end()) {
		return &m_items[it->second];
	}
	return nullptr;
}

void OtbFile::addItem(const OtbItemType &item)
{
	// If lookup tables are clean, incrementally update them (O(1)).
	// Otherwise they're already dirty and will be rebuilt lazily.
	if (!m_lookupDirty) {
		size_t idx = m_items.size();
		if (item.serverId != 0) {
			m_serverIdIndex[item.serverId] = idx;
		}
		if (item.clientId != 0) {
			m_clientIdIndex[item.clientId] = idx;
		}
	}
	m_items.push_back(item);
}

bool OtbFile::removeItemByServerId(uint16_t serverId)
{
	// Use the lookup table if clean for O(1) find, otherwise linear scan.
	if (!m_lookupDirty) {
		auto mapIt = m_serverIdIndex.find(serverId);
		if (mapIt == m_serverIdIndex.end()) {
			return false;
		}
		m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(mapIt->second));
	} else {
		auto it = std::find_if(m_items.begin(), m_items.end(), [serverId](const OtbItemType &item) { return item.serverId == serverId; });
		if (it == m_items.end()) {
			return false;
		}
		m_items.erase(it);
	}

	// Erase shifts all subsequent indices — must do a full rebuild.
	m_lookupDirty = true;
	return true;
}

void OtbFile::createEmpty(uint32_t majorVersion, uint32_t minorVersion, uint32_t buildNumber)
{
	unload();
	m_versionInfo.majorVersion = majorVersion;
	m_versionInfo.minorVersion = minorVersion;
	m_versionInfo.buildNumber = buildNumber;
	m_versionInfo.description = "Hellspawn Editor";
	m_loaded = true;
	// unload() already cleared the maps and set m_lookupDirty = false
}

void OtbFile::rebuildLookupTables() const
{
	m_serverIdIndex.clear();
	m_clientIdIndex.clear();

	m_serverIdIndex.reserve(m_items.size());
	m_clientIdIndex.reserve(m_items.size());

	for (size_t i = 0; i < m_items.size(); ++i) {
		const auto &item = m_items[i];
		if (item.serverId != 0) {
			m_serverIdIndex[item.serverId] = i;
		}
		if (item.clientId != 0) {
			m_clientIdIndex[item.clientId] = i;
		}
	}

	m_lookupDirty = false;
}
