/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <QImage>
#include <cstdint>
#include <cstring>
#include <vector>

constexpr int SPRITE_SIZE = 32;
constexpr int SPRITE_CHANNELS = 4;
constexpr int SPRITE_DATA_SIZE = SPRITE_SIZE * SPRITE_SIZE * SPRITE_CHANNELS;

struct SpritePixels
{
		std::vector<uint8_t> rgba;
		bool transparent = true;

		SpritePixels()
		: rgba(SPRITE_DATA_SIZE, 0)
		, transparent(true)
		{
		}

		bool isEmpty() const
		{
			return transparent;
		}

		QImage toImage() const
		{
			// Use Format_RGBA8888 which stores pixels as R,G,B,A in memory —
			// exactly matching our rgba[] buffer layout.  A single memcpy
			// replaces the previous per-pixel setPixelColor loop (1024 calls).
			QImage image(SPRITE_SIZE, SPRITE_SIZE, QImage::Format_RGBA8888);

			if (transparent) {
				image.fill(Qt::transparent);
				return image;
			}

			// Copy entire pixel buffer into the QImage scanlines.
			// QImage rows may have padding (bytesPerLine >= width*4), so we
			// copy row-by-row to be safe with any Qt build.
			const int rowBytes = SPRITE_SIZE * SPRITE_CHANNELS;
			for (int y = 0; y < SPRITE_SIZE; ++y) {
				std::memcpy(image.scanLine(y), rgba.data() + y * rowBytes, rowBytes);
			}
			return image;
		}

		static SpritePixels fromImage(const QImage &image)
		{
			SpritePixels sprite;
			QImage converted = image.scaled(SPRITE_SIZE, SPRITE_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
			                       .convertToFormat(QImage::Format_RGBA8888);

			// Bulk-copy each scanline from the QImage into our flat rgba buffer,
			// then do a single pass to detect transparency.
			const int rowBytes = SPRITE_SIZE * SPRITE_CHANNELS;
			for (int y = 0; y < SPRITE_SIZE; ++y) {
				std::memcpy(sprite.rgba.data() + y * rowBytes, converted.constScanLine(y), rowBytes);
			}

			// Scan alpha channel to set the transparent flag.
			for (int i = 0; i < SPRITE_SIZE * SPRITE_SIZE; ++i) {
				if (sprite.rgba[i * SPRITE_CHANNELS + 3] != 0) {
					sprite.transparent = false;
					break;
				}
			}
			return sprite;
		}
};

struct SpriteRawEntry
{
		uint8_t colorKeyR = 0xFF;
		uint8_t colorKeyG = 0x00;
		uint8_t colorKeyB = 0xFF;
		std::vector<uint8_t> compressedData;

		bool isNull() const { return compressedData.empty(); }
};
