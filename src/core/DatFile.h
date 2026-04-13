/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/ThingData.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class DatFile
{
	public:
		DatFile() = default;
		~DatFile() = default;

		DatFile(const DatFile &) = delete;
		DatFile &operator=(const DatFile &) = delete;
		DatFile(DatFile &&) noexcept = default;
		DatFile &operator=(DatFile &&) noexcept = default;

		bool load(const std::string &path);
		bool save(const std::string &path) const;
		void unload();

		bool isLoaded() const { return m_loaded; }
		uint32_t signature() const { return m_signature; }
		uint16_t contentRevision() const { return static_cast<uint16_t>(m_signature); }
		const std::string &filePath() const { return m_filePath; }

		void setSignature(uint32_t sig) { m_signature = sig; }

		uint16_t getCategoryCount(ThingCategory category) const;
		uint16_t getFirstId(ThingCategory category) const;
		uint16_t getMaxId(ThingCategory category) const;
		bool isValidId(uint16_t id, ThingCategory category) const;

		const ThingType *getThingType(uint16_t id, ThingCategory category) const;
		ThingType *getThingTypeMut(uint16_t id, ThingCategory category);

		const std::vector<std::shared_ptr<ThingType>> &getThingTypes(ThingCategory category) const;

		bool setThingType(uint16_t id, ThingCategory category, std::shared_ptr<ThingType> thingType);
		uint16_t addThingType(ThingCategory category, std::shared_ptr<ThingType> thingType);
		bool removeThingType(uint16_t id, ThingCategory category);

		void createEmpty(uint32_t signature);

	private:
		void resetThingToDefault(uint16_t id, ThingCategory category);
		std::optional<std::shared_ptr<ThingType>> parseThingType(const uint8_t *data, size_t dataSize, size_t &pos, uint16_t clientId, ThingCategory category) const;

		bool serializeThingType(const ThingType &thing, std::vector<uint8_t> &outData) const;
		bool serializeAnimator(const AnimatorData &animator, std::vector<uint8_t> &outData) const;

		bool m_loaded = false;
		uint32_t m_signature = 0;
		std::string m_filePath;

		std::array<std::vector<std::shared_ptr<ThingType>>, THING_CATEGORY_COUNT> m_thingTypes;
};
