/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "core/SpriteFile.h"
#include "util/Logger.h"
#include "util/Timing.h"

#include <cstring>
#include <fstream>

#include <fmt/core.h>

bool SpriteFile::load(const std::string &path)
{
	unload();

	int64_t startMs = Timing::currentMillis();

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		Log().error("SpriteFile: failed to open '{}'", path);
		return false;
	}

	auto fileSize = file.tellg();
	if (fileSize < static_cast<std::streamoff>(HEADER_SIZE)) {
		Log().error("SpriteFile: file '{}' is too small ({} bytes)", path, static_cast<size_t>(fileSize));
		return false;
	}

	file.seekg(0, std::ios::beg);
	m_fileData.resize(static_cast<size_t>(fileSize));
	if (!file.read(reinterpret_cast<char *>(m_fileData.data()), fileSize)) {
		Log().error("SpriteFile: failed to read '{}' into memory", path);
		m_fileData.clear();
		return false;
	}
	file.close();

	std::memcpy(&m_signature, m_fileData.data(), 4);
	std::memcpy(&m_spriteCount, m_fileData.data() + 4, 4);

	size_t offsetTableEnd = HEADER_SIZE + static_cast<size_t>(m_spriteCount) * OFFSET_ENTRY_SIZE;
	if (m_fileData.size() < offsetTableEnd) {
		Log().error("SpriteFile: file '{}' is truncated (need {} bytes for offset table, have {})", path, offsetTableEnd, m_fileData.size());
		m_fileData.clear();
		m_spriteCount = 0;
		return false;
	}

	m_spriteOffsets.resize(m_spriteCount);
	for (uint32_t i = 0; i < m_spriteCount; ++i) {
		uint32_t offset = 0;
		std::memcpy(&offset, m_fileData.data() + HEADER_SIZE + i * OFFSET_ENTRY_SIZE, 4);
		m_spriteOffsets[i] = offset;
	}

	m_filePath = path;
	m_loaded = true;
	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("SpriteFile: loaded '{}' — signature=0x{:08X}, sprites={} ({}ms)", path, m_signature, m_spriteCount, elapsedMs);
	return true;
}

bool SpriteFile::save(const std::string &path) const
{
	if (!m_loaded) {
		Log().error("SpriteFile: cannot save — no data loaded");
		return false;
	}

	int64_t startMs = Timing::currentMillis();

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		Log().error("SpriteFile: failed to open '{}' for writing", path);
		return false;
	}

	// Write header
	file.write(reinterpret_cast<const char *>(&m_signature), 4);
	file.write(reinterpret_cast<const char *>(&m_spriteCount), 4);

	// Reserve space for offset table — we'll fill it in later
	size_t offsetTablePos = 8;
	std::vector<uint32_t> newOffsets(m_spriteCount, 0);
	// Write placeholder zeros for offsets
	file.write(reinterpret_cast<const char *>(newOffsets.data()), static_cast<std::streamsize>(m_spriteCount * OFFSET_ENTRY_SIZE));

	// Write each sprite's data
	for (uint32_t i = 0; i < m_spriteCount; ++i) {
		uint32_t spriteId = i + 1;

		// Check if we have an override for this sprite
		auto overrideIt = m_overrides.find(spriteId);
		if (overrideIt != m_overrides.end()) {
			const SpriteRawEntry &entry = overrideIt->second;
			if (entry.isNull()) {
				// Cleared sprite — offset stays 0
				continue;
			}
			uint32_t currentPos = static_cast<uint32_t>(file.tellp());
			newOffsets[i] = currentPos;

			file.put(static_cast<char>(entry.colorKeyR));
			file.put(static_cast<char>(entry.colorKeyG));
			file.put(static_cast<char>(entry.colorKeyB));

			uint16_t dataSize = static_cast<uint16_t>(entry.compressedData.size());
			file.write(reinterpret_cast<const char *>(&dataSize), 2);
			if (dataSize > 0) {
				file.write(reinterpret_cast<const char *>(entry.compressedData.data()), dataSize);
			}
		} else {
			// Copy from original file data
			if (i < m_spriteOffsets.size()) {
				uint32_t origOffset = m_spriteOffsets[i];
				if (origOffset == 0) {
					continue;
				}

				if (origOffset + 5 > m_fileData.size()) {
					continue;
				}

				uint32_t currentPos = static_cast<uint32_t>(file.tellp());
				newOffsets[i] = currentPos;

				// Read color key (3 bytes) and data size (2 bytes) from original
				uint8_t ckR = m_fileData[origOffset];
				uint8_t ckG = m_fileData[origOffset + 1];
				uint8_t ckB = m_fileData[origOffset + 2];
				uint16_t dataSize = 0;
				std::memcpy(&dataSize, m_fileData.data() + origOffset + 3, 2);

				file.put(static_cast<char>(ckR));
				file.put(static_cast<char>(ckG));
				file.put(static_cast<char>(ckB));
				file.write(reinterpret_cast<const char *>(&dataSize), 2);

				if (dataSize > 0 && origOffset + 5 + dataSize <= m_fileData.size()) {
					file.write(reinterpret_cast<const char *>(m_fileData.data() + origOffset + 5), dataSize);
				}
			}
		}
	}

	// Go back and write the actual offsets
	file.seekp(static_cast<std::streamoff>(offsetTablePos));
	file.write(reinterpret_cast<const char *>(newOffsets.data()), static_cast<std::streamsize>(m_spriteCount * OFFSET_ENTRY_SIZE));

	file.flush();
	file.close();

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("SpriteFile: saved '{}' — sprites={} ({}ms)", path, m_spriteCount, elapsedMs);
	return true;
}

void SpriteFile::unload()
{
	m_loaded = false;
	m_signature = 0;
	m_spriteCount = 0;
	m_filePath.clear();
	m_fileData.clear();
	m_spriteOffsets.clear();
	m_overrides.clear();
}

SpritePixels SpriteFile::decodeSpriteData(const uint8_t *data, uint16_t dataSize)
{
	SpritePixels pixels;

	int writePos = 0;
	int read = 0;

	while (read < dataSize && writePos < SPRITE_DATA_SIZE) {
		if (read + 4 > dataSize) {
			break;
		}

		uint16_t transparentPixels = 0;
		std::memcpy(&transparentPixels, data + read, 2);
		read += 2;

		uint16_t coloredPixels = 0;
		std::memcpy(&coloredPixels, data + read, 2);
		read += 2;

		for (int i = 0; i < transparentPixels && writePos < SPRITE_DATA_SIZE; ++i) {
			pixels.rgba[writePos + 0] = 0x00;
			pixels.rgba[writePos + 1] = 0x00;
			pixels.rgba[writePos + 2] = 0x00;
			pixels.rgba[writePos + 3] = 0x00;
			writePos += 4;
		}

		if (coloredPixels > 0) {
			pixels.transparent = false;
		}

		for (int i = 0; i < coloredPixels && writePos < SPRITE_DATA_SIZE; ++i) {
			if (read + 3 > dataSize) {
				break;
			}
			// Pixel data is stored as RGB
			pixels.rgba[writePos + 0] = data[read + 0]; // R
			pixels.rgba[writePos + 1] = data[read + 1]; // G
			pixels.rgba[writePos + 2] = data[read + 2]; // B
			pixels.rgba[writePos + 3] = 0xFF;
			read += 3;
			writePos += 4;
		}
	}

	// Fill remaining with transparent
	while (writePos < SPRITE_DATA_SIZE) {
		pixels.rgba[writePos + 0] = 0x00;
		pixels.rgba[writePos + 1] = 0x00;
		pixels.rgba[writePos + 2] = 0x00;
		pixels.rgba[writePos + 3] = 0x00;
		writePos += 4;
	}

	return pixels;
}

std::vector<uint8_t> SpriteFile::encodeSpriteData(const SpritePixels &pixels)
{
	std::vector<uint8_t> result;
	result.reserve(SPRITE_DATA_SIZE);

	int totalPixels = SPRITE_SIZE * SPRITE_SIZE;
	int pos = 0;

	while (pos < totalPixels) {
		// Count transparent pixels
		uint16_t transparentCount = 0;
		while (pos < totalPixels && pixels.rgba[pos * 4 + 3] == 0x00) {
			++transparentCount;
			++pos;
		}

		// Count colored pixels
		uint16_t coloredCount = 0;
		int colorStart = pos;
		while (pos < totalPixels && pixels.rgba[pos * 4 + 3] != 0x00) {
			++coloredCount;
			++pos;
		}

		if (transparentCount == 0 && coloredCount == 0) {
			break;
		}

		// Write transparent count
		result.push_back(static_cast<uint8_t>(transparentCount & 0xFF));
		result.push_back(static_cast<uint8_t>((transparentCount >> 8) & 0xFF));

		// Write colored count
		result.push_back(static_cast<uint8_t>(coloredCount & 0xFF));
		result.push_back(static_cast<uint8_t>((coloredCount >> 8) & 0xFF));

		// Write RGB for each colored pixel
		for (int i = 0; i < coloredCount; ++i) {
			int idx = (colorStart + i) * 4;
			result.push_back(pixels.rgba[idx + 0]); // R
			result.push_back(pixels.rgba[idx + 1]); // G
			result.push_back(pixels.rgba[idx + 2]); // B
		}
	}

	return result;
}

QImage SpriteFile::getSpriteImage(uint32_t id) const
{
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return {};
	}

	auto cacheIt = m_imageCache.find(id);
	if (cacheIt != m_imageCache.end()) {
		return cacheIt->second;
	}

	SpritePixels pixels = getSpritePixels(id);
	QImage image = pixels.toImage();
	m_imageCache[id] = image;
	return image;
}

SpritePixels SpriteFile::getSpritePixels(uint32_t id) const
{
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return {};
	}

	// Check overrides first
	auto overrideIt = m_overrides.find(id);
	if (overrideIt != m_overrides.end()) {
		const SpriteRawEntry &entry = overrideIt->second;
		if (entry.isNull()) {
			return {};
		}
		return decodeSpriteData(entry.compressedData.data(), static_cast<uint16_t>(entry.compressedData.size()));
	}

	// Read from file data
	uint32_t index = id - 1;
	if (index >= m_spriteOffsets.size()) {
		return {};
	}

	uint32_t spriteAddress = m_spriteOffsets[index];
	if (spriteAddress == 0) {
		return {};
	}

	if (spriteAddress + 5 > m_fileData.size()) {
		return {};
	}

	// Skip color key (3 bytes), read data size (2 bytes)
	uint16_t pixelDataSize = 0;
	std::memcpy(&pixelDataSize, m_fileData.data() + spriteAddress + 3, 2);

	if (pixelDataSize == 0) {
		return {};
	}

	size_t dataStart = spriteAddress + 5;
	if (dataStart + pixelDataSize > m_fileData.size()) {
		return {};
	}

	return decodeSpriteData(m_fileData.data() + dataStart, pixelDataSize);
}

SpriteRawEntry SpriteFile::getSpriteRaw(uint32_t id) const
{
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return {};
	}

	auto overrideIt = m_overrides.find(id);
	if (overrideIt != m_overrides.end()) {
		return overrideIt->second;
	}

	uint32_t index = id - 1;
	if (index >= m_spriteOffsets.size()) {
		return {};
	}

	uint32_t spriteAddress = m_spriteOffsets[index];
	if (spriteAddress == 0) {
		return {};
	}

	if (spriteAddress + 5 > m_fileData.size()) {
		return {};
	}

	SpriteRawEntry entry;
	entry.colorKeyR = m_fileData[spriteAddress];
	entry.colorKeyG = m_fileData[spriteAddress + 1];
	entry.colorKeyB = m_fileData[spriteAddress + 2];

	uint16_t pixelDataSize = 0;
	std::memcpy(&pixelDataSize, m_fileData.data() + spriteAddress + 3, 2);

	if (pixelDataSize > 0 && spriteAddress + 5 + pixelDataSize <= m_fileData.size()) {
		entry.compressedData.assign(m_fileData.data() + spriteAddress + 5, m_fileData.data() + spriteAddress + 5 + pixelDataSize);
	}

	return entry;
}

bool SpriteFile::setSpritePixels(uint32_t id, const SpritePixels &pixels)
{
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return false;
	}

	SpriteRawEntry entry;
	entry.colorKeyR = 0xFF;
	entry.colorKeyG = 0x00;
	entry.colorKeyB = 0xFF;
	entry.compressedData = encodeSpriteData(pixels);

	m_overrides[id] = std::move(entry);
	return true;
}

bool SpriteFile::setSpriteImage(uint32_t id, const QImage &image)
{
	return setSpritePixels(id, SpritePixels::fromImage(image));
}

bool SpriteFile::clearSprite(uint32_t id)
{
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return false;
	}

	// Store an empty override
	m_overrides[id] = SpriteRawEntry {};
	m_overrides[id].compressedData.clear();
	return true;
}

uint32_t SpriteFile::addSprite(const SpritePixels &pixels)
{
	if (!m_loaded) {
		return 0;
	}

	++m_spriteCount;
	uint32_t newId = m_spriteCount;
	m_spriteOffsets.push_back(0); // No original offset

	SpriteRawEntry entry;
	entry.colorKeyR = 0xFF;
	entry.colorKeyG = 0x00;
	entry.colorKeyB = 0xFF;
	entry.compressedData = encodeSpriteData(pixels);

	m_overrides[newId] = std::move(entry);
	return newId;
}

uint32_t SpriteFile::addSprite(const QImage &image)
{
	return addSprite(SpritePixels::fromImage(image));
}

QImage SpriteFile::getSpriteImageDirect(uint32_t id) const
{
	// Thread-safe: reads ONLY from immutable m_fileData / m_spriteOffsets.
	// Does NOT touch m_overrides or m_imageCache.
	if (!m_loaded || id == 0 || id > m_spriteCount) {
		return {};
	}

	uint32_t index = id - 1;
	if (index >= m_spriteOffsets.size()) {
		return {};
	}

	uint32_t spriteAddress = m_spriteOffsets[index];
	if (spriteAddress == 0) {
		return {};
	}

	if (spriteAddress + 5 > m_fileData.size()) {
		return {};
	}

	// Skip color key (3 bytes), read data size (2 bytes)
	uint16_t pixelDataSize = 0;
	std::memcpy(&pixelDataSize, m_fileData.data() + spriteAddress + 3, 2);

	if (pixelDataSize == 0) {
		return {};
	}

	size_t dataStart = spriteAddress + 5;
	if (dataStart + pixelDataSize > m_fileData.size()) {
		return {};
	}

	SpritePixels pixels = decodeSpriteData(m_fileData.data() + dataStart, pixelDataSize);
	return pixels.toImage();
}

bool SpriteFile::exportSprite(uint32_t id, const std::string &path) const
{
	QImage image = getSpriteImage(id);
	if (image.isNull()) {
		return false;
	}
	return image.save(QString::fromStdString(path));
}

bool SpriteFile::importSprite(uint32_t id, const std::string &path)
{
	QImage image(QString::fromStdString(path));
	if (image.isNull()) {
		Log().error("SpriteFile: failed to load image from '{}'", path);
		return false;
	}
	return setSpriteImage(id, image);
}

void SpriteFile::createEmpty(uint32_t signature)
{
	unload();
	m_signature = signature;
	m_spriteCount = 0;
	m_loaded = true;
	m_filePath.clear();
}
