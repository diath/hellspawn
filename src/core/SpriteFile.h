/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/SpriteData.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QImage>

class SpriteFile
{
	public:
		SpriteFile() = default;
		~SpriteFile() = default;

		SpriteFile(const SpriteFile &) = delete;
		SpriteFile &operator=(const SpriteFile &) = delete;
		SpriteFile(SpriteFile &&) noexcept = default;
		SpriteFile &operator=(SpriteFile &&) noexcept = default;

		bool load(const std::string &path);
		bool save(const std::string &path) const;
		void unload();

		bool isLoaded() const { return m_loaded; }
		uint32_t signature() const { return m_signature; }
		uint32_t spriteCount() const { return m_spriteCount; }
		const std::string &filePath() const { return m_filePath; }

		void setSignature(uint32_t sig) { m_signature = sig; }

		QImage getSpriteImage(uint32_t id) const;
		SpritePixels getSpritePixels(uint32_t id) const;
		SpriteRawEntry getSpriteRaw(uint32_t id) const;

		/// Thread-safe sprite image accessor that reads ONLY from immutable file
		/// data (m_fileData / m_spriteOffsets).  It does NOT consult the mutable
		/// override map or the mutable image cache, so it can be called from any
		/// thread without synchronisation.
		///
		/// Use this for background icon building.  User-edited (overridden)
		/// sprites are handled separately via the synchronous path on the main
		/// thread.
		QImage getSpriteImageDirect(uint32_t id) const;

		bool setSpritePixels(uint32_t id, const SpritePixels &pixels);
		bool setSpriteImage(uint32_t id, const QImage &image);
		bool clearSprite(uint32_t id);

		uint32_t addSprite(const SpritePixels &pixels);
		uint32_t addSprite(const QImage &image);

		bool exportSprite(uint32_t id, const std::string &path) const;
		bool importSprite(uint32_t id, const std::string &path);

		void createEmpty(uint32_t signature);

	private:
		static SpritePixels decodeSpriteData(const uint8_t *data, uint16_t dataSize);
		static std::vector<uint8_t> encodeSpriteData(const SpritePixels &pixels);

		void loadSpriteRaw(uint32_t id) const;

		bool m_loaded = false;
		uint32_t m_signature = 0;
		uint32_t m_spriteCount = 0;
		std::string m_filePath;

		std::vector<uint8_t> m_fileData;
		std::vector<uint32_t> m_spriteOffsets;

		mutable std::unordered_map<uint32_t, SpriteRawEntry> m_overrides;
		mutable std::unordered_map<uint32_t, QImage> m_imageCache;

		void invalidateCachedImage(uint32_t id) { m_imageCache.erase(id); }

		static constexpr uint32_t HEADER_SIZE = 8;
		static constexpr uint32_t OFFSET_ENTRY_SIZE = 4;
};
