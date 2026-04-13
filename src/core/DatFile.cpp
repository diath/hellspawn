/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "core/DatFile.h"
#include "util/BinaryReader.h"
#include "util/BinaryWriter.h"
#include "util/Logger.h"
#include "util/Timing.h"

#include <cstring>
#include <fstream>
#include <optional>

#include <fmt/core.h>

bool DatFile::load(const std::string &path)
{
	unload();

	int64_t startMs = Timing::currentMillis();

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		Log().error("DatFile: failed to open '{}'", path);
		return false;
	}

	auto fileSize = file.tellg();
	if (fileSize < 12) {
		Log().error("DatFile: file '{}' is too small ({} bytes)", path, static_cast<size_t>(fileSize));
		return false;
	}

	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
	if (!file.read(reinterpret_cast<char *>(fileData.data()), fileSize)) {
		Log().error("DatFile: failed to read '{}'", path);
		return false;
	}
	file.close();

	BinaryReader reader(fileData);

	m_signature = reader.getU32();

	// Read category counts: items, creatures, effects, missiles
	// Each stored as u16; the actual vector size is count + 1
	uint16_t categoryCounts[THING_CATEGORY_COUNT];
	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		categoryCounts[i] = reader.getU16();
	}

	// Allocate vectors: size = count + 1, filled with nullptr
	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		uint16_t count = categoryCounts[i] + 1;
		m_thingTypes[i].resize(count);
	}

	// Parse thing types for each category
	for (int cat = 0; cat < THING_CATEGORY_COUNT; ++cat) {
		ThingCategory category = static_cast<ThingCategory>(cat);
		uint16_t firstId = (category == ThingCategory::Item) ? 100 : 1;

		for (size_t id = firstId; id < m_thingTypes[cat].size(); ++id) {
			size_t pos = reader.tell();

			auto result = parseThingType(fileData.data(), fileData.size(), pos, static_cast<uint16_t>(id), category);
			if (!result) {
				Log().error("DatFile: failed to parse thing type id={} category={} at offset {}", id, cat, reader.tell());
				unload();
				return false;
			}

			reader.seek(pos);
			m_thingTypes[cat][id] = std::move(*result);
		}
	}

	m_filePath = path;
	m_loaded = true;

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("DatFile: loaded '{}' — sig=0x{:08X}, items={}, creatures={}, effects={}, missiles={} ({}ms)", path, m_signature, getCategoryCount(ThingCategory::Item), getCategoryCount(ThingCategory::Creature), getCategoryCount(ThingCategory::Effect), getCategoryCount(ThingCategory::Missile), elapsedMs);
	return true;
}

std::optional<std::shared_ptr<ThingType>> DatFile::parseThingType(const uint8_t *data, size_t dataSize, size_t &pos, uint16_t clientId, ThingCategory category) const
{
	BinaryReader reader(data, dataSize);
	reader.seek(pos);

	auto thingType = std::make_shared<ThingType>();
	thingType->id = clientId;
	thingType->category = category;

	// Read attributes until ThingAttrLast (0xFF)
	bool done = false;
	int count = 0;

	for (int i = 0; i < ThingAttrLast && !done; ++i) {
		if (!reader.canRead(1)) {
			return std::nullopt;
		}

		++count;
		uint8_t attr = reader.getU8();

		if (attr == ThingAttrLast) {
			done = true;
			break;
		}

		switch (attr) {
			case ThingAttrDisplacement: {
				if (!reader.canRead(4))
					return std::nullopt;
				DisplacementData disp;
				disp.x = reader.getU16();
				disp.y = reader.getU16();
				thingType->displacement = disp;
				thingType->setAttr(attr, disp);
				break;
			}
			case ThingAttrLight: {
				if (!reader.canRead(4))
					return std::nullopt;
				LightData light;
				light.intensity = reader.getU16();
				light.color = reader.getU16();
				thingType->setAttr(attr, light);
				break;
			}
			case ThingAttrMarket: {
				if (!reader.canRead(10))
					return std::nullopt;
				MarketData market;
				market.category = reader.getU16();
				market.tradeAs = reader.getU16();
				market.showAs = reader.getU16();
				market.name = reader.getString();
				market.restrictVocation = reader.getU16();
				market.requiredLevel = reader.getU16();
				thingType->setAttr(attr, market);
				break;
			}
			case ThingAttrElevation: {
				if (!reader.canRead(2))
					return std::nullopt;
				uint16_t elev = reader.getU16();
				thingType->elevation = elev;
				thingType->setAttr(attr, elev);
				break;
			}
			case ThingAttrGround:
			case ThingAttrWritable:
			case ThingAttrWritableOnce:
			case ThingAttrMinimapColor:
			case ThingAttrCloth:
			case ThingAttrLensHelp:
			case ThingAttrUsable: {
				if (!reader.canRead(2))
					return std::nullopt;
				uint16_t val = reader.getU16();
				thingType->setAttr(attr, val);
				break;
			}
			case ThingAttrTopEffect:
			default: {
				thingType->setAttr(attr, true);
				break;
			}
		}
	}

	if (!done) {
		Log().error("DatFile: corrupt thing data (id={}, category={}, count={}) — missing terminator", clientId, static_cast<int>(category), count);
		return std::nullopt;
	}

	// Read frame groups
	bool hasFrameGroups = (category == ThingCategory::Creature);
	uint8_t groupCount = 1;
	if (hasFrameGroups) {
		if (!reader.canRead(1))
			return std::nullopt;
		groupCount = reader.getU8();
	}

	for (int g = 0; g < groupCount; ++g) {
		FrameGroupType fgType = FrameGroupType::Default;
		if (hasFrameGroups) {
			if (!reader.canRead(1))
				return std::nullopt;
			fgType = static_cast<FrameGroupType>(reader.getU8());
		}

		auto group = std::make_unique<FrameGroup>();
		group->type = fgType;

		if (!reader.canRead(2))
			return std::nullopt;
		group->width = reader.getU8();
		group->height = reader.getU8();

		if (group->width > 1 || group->height > 1) {
			if (!reader.canRead(1))
				return std::nullopt;
			group->exactSize = reader.getU8();
		} else {
			group->exactSize = 32;
		}

		if (!reader.canRead(5))
			return std::nullopt;
		group->layers = reader.getU8();
		group->patternX = reader.getU8();
		group->patternY = reader.getU8();
		group->patternZ = reader.getU8();
		group->animationPhases = reader.getU8();

		if (group->animationPhases > 1) {
			group->isAnimation = true;
			group->animator = std::make_unique<AnimatorData>();

			// Read animator data
			if (!reader.canRead(6))
				return std::nullopt;
			group->animator->async = (reader.getU8() == 0);
			group->animator->loopCount = reader.get32();
			group->animator->startPhase = reader.get8();

			for (int p = 0; p < group->animationPhases; ++p) {
				if (!reader.canRead(8))
					return std::nullopt;
				uint32_t minDuration = reader.getU32();
				uint32_t maxDuration = reader.getU32();
				group->animator->phaseDurations.emplace_back(minDuration, maxDuration);
			}
		}

		int totalSprites = group->totalSprites();
		if (totalSprites > 4096) {
			Log().error("DatFile: thing type id={} has too many sprites ({})", clientId, totalSprites);
			return std::nullopt;
		}

		if (!reader.canRead(static_cast<size_t>(totalSprites) * 4))
			return std::nullopt;
		group->spriteIds.resize(totalSprites);
		for (int s = 0; s < totalSprites; ++s) {
			group->spriteIds[s] = reader.getU32();
		}

		int fgIdx = static_cast<int>(group->type);
		if (fgIdx >= 0 && fgIdx < FRAME_GROUP_COUNT && !thingType->frameGroups[fgIdx]) {
			thingType->frameGroups[fgIdx] = std::move(group);
		}
	}

	pos = reader.tell();
	return thingType;
}

bool DatFile::save(const std::string &path) const
{
	if (!m_loaded) {
		Log().error("DatFile: cannot save — no data loaded");
		return false;
	}

	int64_t startMs = Timing::currentMillis();

	std::vector<uint8_t> outData;
	outData.reserve(1024 * 1024);

	BinaryWriter writer;
	writer.reserve(1024 * 1024);

	// Write signature
	writer.addU32(m_signature);

	// Write category counts (each is vector.size() - 1)
	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		uint16_t count = 0;
		if (!m_thingTypes[i].empty()) {
			count = static_cast<uint16_t>(m_thingTypes[i].size() - 1);
		}
		writer.addU16(count);
	}

	// Write thing types for each category
	for (int cat = 0; cat < THING_CATEGORY_COUNT; ++cat) {
		ThingCategory category = static_cast<ThingCategory>(cat);
		uint16_t firstId = (category == ThingCategory::Item) ? 100 : 1;

		for (size_t id = firstId; id < m_thingTypes[cat].size(); ++id) {
			const auto &thingPtr = m_thingTypes[cat][id];
			if (thingPtr) {
				std::vector<uint8_t> thingData;
				if (!serializeThingType(*thingPtr, thingData)) {
					Log().error("DatFile: failed to serialize thing type id={} cat={}", id, cat);
					return false;
				}
				writer.addBytes(thingData);
			} else {
				// Write an empty thing type (just the terminator, then minimal frame group)
				writer.addU8(ThingAttrLast);
				// width=1, height=1
				writer.addU8(1);
				writer.addU8(1);
				// layers=1, patternX=1, patternY=1, patternZ=1, animationPhases=1
				writer.addU8(1);
				writer.addU8(1);
				writer.addU8(1);
				writer.addU8(1);
				writer.addU8(1);
				// 1 sprite id = 0
				writer.addU32(0);
			}
		}
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		Log().error("DatFile: failed to open '{}' for writing", path);
		return false;
	}

	const auto &buf = writer.buffer();
	file.write(reinterpret_cast<const char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
	file.flush();
	file.close();

	int64_t elapsedMs = Timing::currentMillis() - startMs;
	Log().info("DatFile: saved '{}' ({} bytes, {}ms)", path, buf.size(), elapsedMs);
	return true;
}

bool DatFile::serializeThingType(const ThingType &thing, std::vector<uint8_t> &outData) const
{
	BinaryWriter writer;

	// Write attributes
	// We need to write them in a specific order to match the reference.
	// The order doesn't strictly matter for the parser (it reads until 0xFF),
	// but let's iterate attributes map.
	for (const auto &[attr, value]: thing.attributes) {
		writer.addU8(attr);

		switch (attr) {
			case ThingAttrDisplacement: {
				const auto &disp = std::get<DisplacementData>(value);
				writer.addU16(disp.x);
				writer.addU16(disp.y);
				break;
			}
			case ThingAttrLight: {
				const auto &light = std::get<LightData>(value);
				writer.addU16(light.intensity);
				writer.addU16(light.color);
				break;
			}
			case ThingAttrMarket: {
				const auto &market = std::get<MarketData>(value);
				writer.addU16(market.category);
				writer.addU16(market.tradeAs);
				writer.addU16(market.showAs);
				writer.addString(market.name);
				writer.addU16(market.restrictVocation);
				writer.addU16(market.requiredLevel);
				break;
			}
			case ThingAttrElevation:
			case ThingAttrGround:
			case ThingAttrWritable:
			case ThingAttrWritableOnce:
			case ThingAttrMinimapColor:
			case ThingAttrCloth:
			case ThingAttrLensHelp:
			case ThingAttrUsable: {
				writer.addU16(std::get<uint16_t>(value));
				break;
			}
			default: {
				// Boolean attribute — no additional data
				break;
			}
		}
	}

	// Write attribute terminator
	writer.addU8(ThingAttrLast);

	// Write frame groups
	bool hasFrameGroups = (thing.category == ThingCategory::Creature);
	int groupCount = 0;
	for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
		if (thing.frameGroups[i]) {
			++groupCount;
		}
	}

	if (groupCount == 0) {
		groupCount = 1;
	}

	if (hasFrameGroups) {
		writer.addU8(static_cast<uint8_t>(groupCount));
	}

	bool wroteAny = false;
	for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
		const auto &group = thing.frameGroups[i];
		if (!group)
			continue;

		wroteAny = true;

		if (hasFrameGroups) {
			writer.addU8(static_cast<uint8_t>(group->type));
		}

		writer.addU8(group->width);
		writer.addU8(group->height);

		if (group->width > 1 || group->height > 1) {
			writer.addU8(group->exactSize);
		}

		writer.addU8(group->layers);
		writer.addU8(group->patternX);
		writer.addU8(group->patternY);
		writer.addU8(group->patternZ);
		writer.addU8(group->animationPhases);

		if (group->animationPhases > 1 && group->animator) {
			writer.addU8(group->animator->async ? 0 : 1);
			writer.add32(group->animator->loopCount);
			writer.add8(group->animator->startPhase);

			for (const auto &[minDur, maxDur]: group->animator->phaseDurations) {
				writer.addU32(minDur);
				writer.addU32(maxDur);
			}
		}

		for (uint32_t sprId: group->spriteIds) {
			writer.addU32(sprId);
		}
	}

	// If no frame group existed, write a default empty one
	if (!wroteAny) {
		if (hasFrameGroups) {
			writer.addU8(static_cast<uint8_t>(FrameGroupType::Default));
		}
		writer.addU8(1); // width
		writer.addU8(1); // height
		writer.addU8(1); // layers
		writer.addU8(1); // patternX
		writer.addU8(1); // patternY
		writer.addU8(1); // patternZ
		writer.addU8(1); // animationPhases
		writer.addU32(0); // sprite id
	}

	outData = writer.takeBuffer();
	return true;
}

bool DatFile::serializeAnimator(const AnimatorData & /*animator*/, std::vector<uint8_t> & /*outData*/) const
{
	// Animator serialization is inlined in serializeThingType above.
	// This function exists for interface completeness.
	return true;
}

void DatFile::unload()
{
	m_loaded = false;
	m_signature = 0;
	m_filePath.clear();
	for (auto &vec: m_thingTypes) {
		vec.clear();
	}
}

uint16_t DatFile::getCategoryCount(ThingCategory category) const
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return 0;
	if (m_thingTypes[cat].empty())
		return 0;
	return static_cast<uint16_t>(m_thingTypes[cat].size() - 1);
}

uint16_t DatFile::getFirstId(ThingCategory category) const
{
	return (category == ThingCategory::Item) ? 100 : 1;
}

uint16_t DatFile::getMaxId(ThingCategory category) const
{
	return getCategoryCount(category);
}

bool DatFile::isValidId(uint16_t id, ThingCategory category) const
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return false;
	return id >= getFirstId(category) && id < m_thingTypes[cat].size();
}

const ThingType *DatFile::getThingType(uint16_t id, ThingCategory category) const
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return nullptr;
	if (id >= m_thingTypes[cat].size())
		return nullptr;
	return m_thingTypes[cat][id].get();
}

ThingType *DatFile::getThingTypeMut(uint16_t id, ThingCategory category)
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return nullptr;
	if (id >= m_thingTypes[cat].size())
		return nullptr;
	return m_thingTypes[cat][id].get();
}

const std::vector<std::shared_ptr<ThingType>> &DatFile::getThingTypes(ThingCategory category) const
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT) {
		static const std::vector<std::shared_ptr<ThingType>> empty;
		return empty;
	}
	return m_thingTypes[cat];
}

bool DatFile::setThingType(uint16_t id, ThingCategory category, std::shared_ptr<ThingType> thingType)
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return false;
	if (id >= m_thingTypes[cat].size())
		return false;

	thingType->id = id;
	thingType->category = category;
	m_thingTypes[cat][id] = std::move(thingType);
	return true;
}

uint16_t DatFile::addThingType(ThingCategory category, std::shared_ptr<ThingType> thingType)
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return 0;

	uint16_t newId = static_cast<uint16_t>(m_thingTypes[cat].size());
	thingType->id = newId;
	thingType->category = category;
	m_thingTypes[cat].push_back(std::move(thingType));
	return newId;
}

bool DatFile::removeThingType(uint16_t id, ThingCategory category)
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return false;
	if (id >= m_thingTypes[cat].size())
		return false;

	uint16_t firstId = getFirstId(category);
	if (id < firstId)
		return false;

	// Don't allow shrinking below the minimum reserved size
	if (m_thingTypes[cat].size() <= static_cast<size_t>(firstId))
		return false;

	if (id == static_cast<uint16_t>(m_thingTypes[cat].size() - 1)) {
		// End of array: remove the element and downsize the vector.
		// This automatically keeps getCategoryCount() / getMaxId() in sync
		// since they derive from vector size.
		m_thingTypes[cat].pop_back();
	} else {
		// Mid-array: reset to a default-constructed ThingType so the
		// flat array stays fully populated with no null entries.
		resetThingToDefault(id, category);
	}

	return true;
}

void DatFile::resetThingToDefault(uint16_t id, ThingCategory category)
{
	int cat = static_cast<int>(category);

	auto thing = std::make_shared<ThingType>();
	thing->id = id;
	thing->category = category;

	// Create a minimal default FrameGroup so the entry serializes correctly
	// and is indistinguishable from a freshly-added empty thing.
	auto group = std::make_unique<FrameGroup>();
	group->type = FrameGroupType::Default;
	group->width = 1;
	group->height = 1;
	group->exactSize = 32;
	group->layers = 1;
	group->patternX = 1;
	group->patternY = 1;
	group->patternZ = 1;
	group->animationPhases = 1;
	group->spriteIds.resize(1, 0);
	thing->frameGroups[0] = std::move(group);

	m_thingTypes[cat][id] = std::move(thing);
}

void DatFile::createEmpty(uint32_t signature)
{
	unload();
	m_signature = signature;
	m_loaded = true;

	// Items start at 100, so we need at least 100 slots
	// Creatures, effects, missiles start at 1, need at least 1 slot
	m_thingTypes[static_cast<int>(ThingCategory::Item)].resize(100);
	m_thingTypes[static_cast<int>(ThingCategory::Creature)].resize(1);
	m_thingTypes[static_cast<int>(ThingCategory::Effect)].resize(1);
	m_thingTypes[static_cast<int>(ThingCategory::Missile)].resize(1);
}
