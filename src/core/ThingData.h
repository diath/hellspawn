/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

enum class ThingCategory : uint8_t
{
	Item = 0,
	Creature,
	Effect,
	Missile,
	Invalid,
	Last = Invalid
};

constexpr int THING_CATEGORY_COUNT = static_cast<int>(ThingCategory::Last);

inline const char *thingCategoryName(ThingCategory cat)
{
	switch (cat) {
		case ThingCategory::Item:
			return "Items";
		case ThingCategory::Creature:
			return "Creatures";
		case ThingCategory::Effect:
			return "Effects";
		case ThingCategory::Missile:
			return "Missiles";
		default:
			return "Invalid";
	}
}

inline const char *thingCategoryLabel(ThingCategory cat)
{
	switch (cat) {
		case ThingCategory::Item:
			return "Item";
		case ThingCategory::Creature:
			return "Creature";
		case ThingCategory::Effect:
			return "Effect";
		case ThingCategory::Missile:
			return "Missile";
		default:
			return "Unkniown";
	}
}

inline ThingCategory thingCategoryFromIndex(int index)
{
	if (index >= 0 && index < THING_CATEGORY_COUNT) {
		return static_cast<ThingCategory>(index);
	}
	return ThingCategory::Invalid;
}

enum class FrameGroupType : uint8_t
{
	Default = 0,
	Idle = 0,
	Walking = 1,
};

constexpr int FRAME_GROUP_COUNT = 2;

enum ThingAttr : uint8_t
{
	ThingAttrGround = 0,
	ThingAttrGroundBorder = 1,
	ThingAttrOnBottom = 2,
	ThingAttrOnTop = 3,
	ThingAttrContainer = 4,
	ThingAttrStackable = 5,
	ThingAttrForceUse = 6,
	ThingAttrMultiUse = 7,
	ThingAttrWritable = 8,
	ThingAttrWritableOnce = 9,
	ThingAttrFluidContainer = 10,
	ThingAttrSplash = 11,
	ThingAttrNotWalkable = 12,
	ThingAttrNotMoveable = 13,
	ThingAttrBlockProjectile = 14,
	ThingAttrNotPathable = 15,
	ThingAttrNoMoveAnimation = 16,
	ThingAttrPickupable = 17,
	ThingAttrHangable = 18,
	ThingAttrHookSouth = 19,
	ThingAttrHookEast = 20,
	ThingAttrRotateable = 21,
	ThingAttrLight = 22,
	ThingAttrDontHide = 23,
	ThingAttrTranslucent = 24,
	ThingAttrDisplacement = 25,
	ThingAttrElevation = 26,
	ThingAttrLyingCorpse = 27,
	ThingAttrAnimateAlways = 28,
	ThingAttrMinimapColor = 29,
	ThingAttrLensHelp = 30,
	ThingAttrFullGround = 31,
	ThingAttrLook = 32,
	ThingAttrCloth = 33,
	ThingAttrMarket = 34,
	ThingAttrUsable = 35,
	ThingAttrWrapable = 36,
	ThingAttrUnwrapable = 37,
	ThingAttrTopEffect = 38,

	ThingAttrOpacity = 100,
	ThingAttrNotPreWalkable = 101,

	ThingAttrFloorChange = 252,
	ThingAttrChargeable = 254,
	ThingAttrLast = 255
};

inline const char *thingAttrName(uint8_t attr)
{
	switch (attr) {
		case ThingAttrGround:
			return "Ground";
		case ThingAttrGroundBorder:
			return "Ground Border";
		case ThingAttrOnBottom:
			return "On Bottom";
		case ThingAttrOnTop:
			return "On Top";
		case ThingAttrContainer:
			return "Container";
		case ThingAttrStackable:
			return "Stackable";
		case ThingAttrForceUse:
			return "Force Use";
		case ThingAttrMultiUse:
			return "Multi Use";
		case ThingAttrWritable:
			return "Writable";
		case ThingAttrWritableOnce:
			return "Writable Once";
		case ThingAttrFluidContainer:
			return "Fluid Container";
		case ThingAttrSplash:
			return "Splash";
		case ThingAttrNotWalkable:
			return "Not Walkable";
		case ThingAttrNotMoveable:
			return "Not Moveable";
		case ThingAttrBlockProjectile:
			return "Block Projectile";
		case ThingAttrNotPathable:
			return "Not Pathable";
		case ThingAttrNoMoveAnimation:
			return "No Move Animation";
		case ThingAttrPickupable:
			return "Pickupable";
		case ThingAttrHangable:
			return "Hangable";
		case ThingAttrHookSouth:
			return "Hook South";
		case ThingAttrHookEast:
			return "Hook East";
		case ThingAttrRotateable:
			return "Rotateable";
		case ThingAttrLight:
			return "Light";
		case ThingAttrDontHide:
			return "Don't Hide";
		case ThingAttrTranslucent:
			return "Translucent";
		case ThingAttrDisplacement:
			return "Displacement";
		case ThingAttrElevation:
			return "Elevation";
		case ThingAttrLyingCorpse:
			return "Lying Corpse";
		case ThingAttrAnimateAlways:
			return "Animate Always";
		case ThingAttrMinimapColor:
			return "Minimap Color";
		case ThingAttrLensHelp:
			return "Lens Help";
		case ThingAttrFullGround:
			return "Full Ground";
		case ThingAttrLook:
			return "Ignore Look";
		case ThingAttrCloth:
			return "Cloth";
		case ThingAttrMarket:
			return "Market";
		case ThingAttrUsable:
			return "Usable";
		case ThingAttrWrapable:
			return "Wrapable";
		case ThingAttrUnwrapable:
			return "Unwrapable";
		case ThingAttrTopEffect:
			return "Top Effect";
		case ThingAttrChargeable:
			return "Chargeable";
		default:
			return "Unknown";
	}
}

enum class ThingAttrValueType : uint8_t
{
	Boolean,
	U16,
	Light,
	Displacement,
	Market,
};

inline ThingAttrValueType thingAttrValueType(uint8_t attr)
{
	switch (attr) {
		case ThingAttrDisplacement:
			return ThingAttrValueType::Displacement;
		case ThingAttrLight:
			return ThingAttrValueType::Light;
		case ThingAttrMarket:
			return ThingAttrValueType::Market;
		case ThingAttrGround:
		case ThingAttrWritable:
		case ThingAttrWritableOnce:
		case ThingAttrMinimapColor:
		case ThingAttrCloth:
		case ThingAttrLensHelp:
		case ThingAttrUsable:
		case ThingAttrElevation:
			return ThingAttrValueType::U16;
		default:
			return ThingAttrValueType::Boolean;
	}
}

struct LightData
{
		uint16_t intensity = 0;
		uint16_t color = 215;
};

struct DisplacementData
{
		uint16_t x = 0;
		uint16_t y = 0;
};

struct MarketData
{
		uint16_t category = 0;
		uint16_t tradeAs = 0;
		uint16_t showAs = 0;
		std::string name;
		uint16_t restrictVocation = 0;
		uint16_t requiredLevel = 0;
};

using ThingAttrValue = std::variant<bool, uint16_t, LightData, DisplacementData, MarketData>;

struct AnimatorData
{
		bool async = true;
		int32_t loopCount = 0;
		int8_t startPhase = -1;
		std::vector<std::tuple<uint32_t, uint32_t>> phaseDurations;

		uint32_t totalDuration() const
		{
			uint32_t total = 0;
			for (auto &[minDur, maxDur]: phaseDurations) {
				total += maxDur;
			}
			return total;
		}
};

struct FrameGroup
{
		FrameGroupType type = FrameGroupType::Default;
		uint8_t width = 1;
		uint8_t height = 1;
		uint8_t exactSize = 32;
		uint8_t layers = 1;
		uint8_t patternX = 1;
		uint8_t patternY = 1;
		uint8_t patternZ = 1;
		uint8_t animationPhases = 1;
		bool isAnimation = false;
		std::unique_ptr<AnimatorData> animator;
		std::vector<uint32_t> spriteIds;

		int totalSprites() const
		{
			return static_cast<int>(width) * height * layers * patternX * patternY * patternZ * animationPhases;
		}

		FrameGroup() = default;

		FrameGroup(const FrameGroup &other)
		: type(other.type)
		, width(other.width)
		, height(other.height)
		, exactSize(other.exactSize)
		, layers(other.layers)
		, patternX(other.patternX)
		, patternY(other.patternY)
		, patternZ(other.patternZ)
		, animationPhases(other.animationPhases)
		, isAnimation(other.isAnimation)
		, spriteIds(other.spriteIds)
		{
			if (other.animator) {
				animator = std::make_unique<AnimatorData>(*other.animator);
			}
		}

		FrameGroup &operator=(const FrameGroup &other)
		{
			if (this != &other) {
				type = other.type;
				width = other.width;
				height = other.height;
				exactSize = other.exactSize;
				layers = other.layers;
				patternX = other.patternX;
				patternY = other.patternY;
				patternZ = other.patternZ;
				animationPhases = other.animationPhases;
				isAnimation = other.isAnimation;
				spriteIds = other.spriteIds;
				if (other.animator) {
					animator = std::make_unique<AnimatorData>(*other.animator);
				} else {
					animator.reset();
				}
			}
			return *this;
		}

		FrameGroup(FrameGroup &&) noexcept = default;
		FrameGroup &operator=(FrameGroup &&) noexcept = default;
};

struct ThingType
{
		uint16_t id = 0;
		ThingCategory category = ThingCategory::Invalid;

		std::unordered_map<uint8_t, ThingAttrValue> attributes;
		uint16_t elevation = 0;
		DisplacementData displacement;

		std::array<std::unique_ptr<FrameGroup>, FRAME_GROUP_COUNT> frameGroups;

		bool hasAttr(uint8_t attr) const
		{
			return attributes.find(attr) != attributes.end();
		}

		void setAttr(uint8_t attr, ThingAttrValue value)
		{
			attributes[attr] = std::move(value);
		}

		void removeAttr(uint8_t attr)
		{
			attributes.erase(attr);
		}

		uint16_t getU16Attr(uint8_t attr) const
		{
			auto it = attributes.find(attr);
			if (it != attributes.end() && std::holds_alternative<uint16_t>(it->second)) {
				return std::get<uint16_t>(it->second);
			}
			return 0;
		}

		LightData getLightAttr() const
		{
			auto it = attributes.find(ThingAttrLight);
			if (it != attributes.end() && std::holds_alternative<LightData>(it->second)) {
				return std::get<LightData>(it->second);
			}
			return {};
		}

		MarketData getMarketAttr() const
		{
			auto it = attributes.find(ThingAttrMarket);
			if (it != attributes.end() && std::holds_alternative<MarketData>(it->second)) {
				return std::get<MarketData>(it->second);
			}
			return {};
		}

		const FrameGroup *getFrameGroup(FrameGroupType preferred = FrameGroupType::Default) const
		{
			int idx = static_cast<int>(preferred);
			if (idx >= 0 && idx < FRAME_GROUP_COUNT && frameGroups[idx]) {
				return frameGroups[idx].get();
			}
			for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
				if (frameGroups[i]) {
					return frameGroups[i].get();
				}
			}
			return nullptr;
		}

		FrameGroup *getFrameGroupMut(FrameGroupType preferred = FrameGroupType::Default)
		{
			int idx = static_cast<int>(preferred);
			if (idx >= 0 && idx < FRAME_GROUP_COUNT && frameGroups[idx]) {
				return frameGroups[idx].get();
			}
			for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
				if (frameGroups[i]) {
					return frameGroups[i].get();
				}
			}
			return nullptr;
		}

		const std::vector<uint32_t> &getSpriteIds(FrameGroupType type = FrameGroupType::Default) const
		{
			const auto *group = getFrameGroup(type);
			if (group) {
				return group->spriteIds;
			}
			static const std::vector<uint32_t> empty;
			return empty;
		}

		uint32_t getFirstSpriteId() const
		{
			const auto &ids = getSpriteIds();
			if (!ids.empty()) {
				return ids[0];
			}
			return 0;
		}

		ThingType() = default;

		ThingType(const ThingType &other)
		: id(other.id)
		, category(other.category)
		, attributes(other.attributes)
		, elevation(other.elevation)
		, displacement(other.displacement)
		{
			for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
				if (other.frameGroups[i]) {
					frameGroups[i] = std::make_unique<FrameGroup>(*other.frameGroups[i]);
				}
			}
		}

		ThingType &operator=(const ThingType &other)
		{
			if (this != &other) {
				id = other.id;
				category = other.category;
				attributes = other.attributes;
				elevation = other.elevation;
				displacement = other.displacement;
				for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
					if (other.frameGroups[i]) {
						frameGroups[i] = std::make_unique<FrameGroup>(*other.frameGroups[i]);
					} else {
						frameGroups[i].reset();
					}
				}
			}
			return *this;
		}

		ThingType(ThingType &&) noexcept = default;
		ThingType &operator=(ThingType &&) noexcept = default;
};

struct OtbItemType
{
		uint16_t serverId = 0;
		uint16_t clientId = 0;
		uint8_t group = 0;
		uint32_t flags = 0;
		uint16_t speed = 0;
		uint16_t wareId = 0;
		uint8_t lightLevel = 0;
		uint8_t lightColor = 0;
		uint8_t alwaysOnTopOrder = 0;
		std::string name;
};

struct OtbVersionInfo
{
		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
		uint32_t buildNumber = 0;
		std::string description;
};

enum OtbItemGroup : uint8_t
{
	OtbGroupNone = 0,
	OtbGroupGround,
	OtbGroupContainer,
	OtbGroupWeapon,
	OtbGroupAmmunition,
	OtbGroupArmor,
	OtbGroupCharges,
	OtbGroupTeleport,
	OtbGroupMagicField,
	OtbGroupWriteable,
	OtbGroupKey,
	OtbGroupSplash,
	OtbGroupFluid,
	OtbGroupDoor,
	OtbGroupDeprecated,
	OtbGroupLast
};

enum OtbItemFlags : uint32_t
{
	OtbFlagBlockSolid = 1 << 0,
	OtbFlagBlockProjectile = 1 << 1,
	OtbFlagBlockPathFind = 1 << 2,
	OtbFlagHasHeight = 1 << 3,
	OtbFlagUseable = 1 << 4,
	OtbFlagPickupable = 1 << 5,
	OtbFlagMoveable = 1 << 6,
	OtbFlagStackable = 1 << 7,
	OtbFlagFloorChangeDown = 1 << 8,
	OtbFlagFloorChangeNorth = 1 << 9,
	OtbFlagFloorChangeEast = 1 << 10,
	OtbFlagFloorChangeSouth = 1 << 11,
	OtbFlagFloorChangeWest = 1 << 12,
	OtbFlagAlwaysOnTop = 1 << 13,
	OtbFlagReadable = 1 << 14,
	OtbFlagRotatable = 1 << 15,
	OtbFlagHangable = 1 << 16,
	OtbFlagVertical = 1 << 17,
	OtbFlagHorizontal = 1 << 18,
	OtbFlagCannotDecay = 1 << 19,
	OtbFlagAllowDistRead = 1 << 20,
	OtbFlagUnused = 1 << 21,
	OtbFlagClientCharges = 1 << 22,
	OtbFlagLookThrough = 1 << 23,
	OtbFlagAnimation = 1 << 24,
	OtbFlagFullTile = 1 << 25,
	OtbFlagForceUse = 1 << 26,
};

enum OtbItemAttr : uint8_t
{
	OtbAttrFirst = 0x10,
	OtbAttrServerId = 0x10,
	OtbAttrClientId = 0x11,
	OtbAttrName = 0x12,
	OtbAttrDesc = 0x13,
	OtbAttrSpeed = 0x14,
	OtbAttrSlot = 0x15,
	OtbAttrMaxItems = 0x16,
	OtbAttrWeight = 0x17,
	OtbAttrWeapon = 0x18,
	OtbAttrAmmunition = 0x19,
	OtbAttrArmor = 0x1A,
	OtbAttrMagicLevel = 0x1B,
	OtbAttrMagicField = 0x1C,
	OtbAttrWritable = 0x1D,
	OtbAttrRotateTo = 0x1E,
	OtbAttrDecay = 0x1F,
	OtbAttrSpriteHash = 0x20,
	OtbAttrMinimapColor = 0x21,
	OtbAttr07 = 0x22,
	OtbAttr08 = 0x23,
	OtbAttrLight = 0x24,
	OtbAttrDecay2 = 0x25,
	OtbAttrWeapon2 = 0x26,
	OtbAttrAmmunition2 = 0x27,
	OtbAttrArmor2 = 0x28,
	OtbAttrWritable2 = 0x29,
	OtbAttrLight2 = 0x2A,
	OtbAttrTopOrder = 0x2B,
	OtbAttrWritable3 = 0x2C,
	OtbAttrWareId = 0x2D,
	OtbAttrLast = 0x2E,
};

enum OtbRootAttr : uint8_t
{
	OtbRootAttrVersion = 0x01,
};

constexpr uint8_t OTB_ESCAPE_CHAR = 0xFD;
constexpr uint8_t OTB_NODE_START = 0xFE;
constexpr uint8_t OTB_NODE_END = 0xFF;
