/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/ThingData.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class OtbFile
{
	public:
		OtbFile() = default;
		~OtbFile() = default;

		OtbFile(const OtbFile &) = delete;
		OtbFile &operator=(const OtbFile &) = delete;
		OtbFile(OtbFile &&) noexcept = default;
		OtbFile &operator=(OtbFile &&) noexcept = default;

		bool load(const std::string &path);
		bool save(const std::string &path) const;
		void unload();

		bool isLoaded() const { return m_loaded; }
		const std::string &filePath() const { return m_filePath; }

		const OtbVersionInfo &versionInfo() const { return m_versionInfo; }
		void setVersionInfo(const OtbVersionInfo &info) { m_versionInfo = info; }

		size_t itemCount() const { return m_items.size(); }

		const std::vector<OtbItemType> &items() const { return m_items; }

		/// Returns a mutable reference to the items vector.
		/// WARNING: After modifying the vector through this reference, callers MUST
		/// call invalidateLookupTables() so that subsequent lookups by serverId or
		/// clientId return correct results.
		std::vector<OtbItemType> &itemsMut()
		{
			invalidateLookupTables();
			return m_items;
		}

		const OtbItemType *getItemByServerId(uint16_t serverId) const;
		OtbItemType *getItemByServerIdMut(uint16_t serverId);
		const OtbItemType *getItemByClientId(uint16_t clientId) const;

		void addItem(const OtbItemType &item);
		bool removeItemByServerId(uint16_t serverId);

		void createEmpty(uint32_t majorVersion, uint32_t minorVersion, uint32_t buildNumber);

		/// Force a rebuild of the lookup tables on the next ID-based query.
		/// Call this after mutating items through itemsMut() or items() in ways
		/// that change serverId/clientId values.
		void invalidateLookupTables() { m_lookupDirty = true; }

	private:
		struct OtbNode
		{
				uint8_t type = 0;
				std::vector<uint8_t> props;
				std::vector<OtbNode> children;
		};

		bool parseNodes(const std::vector<uint8_t> &fileData, size_t &pos, OtbNode &node) const;
		bool parseRootNode(const OtbNode &root);
		bool parseItemNode(const OtbNode &node);

		void serializeNode(const OtbNode &node, std::vector<uint8_t> &outData) const;
		void writeEscaped(std::vector<uint8_t> &outData, const uint8_t *src, size_t len) const;

		OtbNode buildRootNode() const;
		OtbNode buildItemNode(const OtbItemType &item) const;

		/// Rebuild both serverId and clientId lookup tables from m_items.
		void rebuildLookupTables() const;

		/// Ensure lookup tables are up-to-date (lazy rebuild).
		void ensureLookupTables() const
		{
			if (m_lookupDirty) {
				rebuildLookupTables();
			}
		}

		bool m_loaded = false;
		std::string m_filePath;
		OtbVersionInfo m_versionInfo;
		std::vector<OtbItemType> m_items;

		// Lookup tables: map ID -> index into m_items for O(1) lookups.
		// Marked mutable so const lookup methods can lazily rebuild them.
		mutable std::unordered_map<uint16_t, size_t> m_serverIdIndex;
		mutable std::unordered_map<uint16_t, size_t> m_clientIdIndex;
		mutable bool m_lookupDirty = true;
};
