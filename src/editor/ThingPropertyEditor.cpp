/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/ThingPropertyEditor.h"
#include "core/SpriteFile.h"
#include "widgets/AnimationPreview.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

ThingPropertyEditor::ThingPropertyEditor(QWidget *parent)
: QWidget(parent)
{
	setupUi();
}

void ThingPropertyEditor::setThingType(ThingType *thingType, const SpriteFile *spriteFile)
{
	m_thingType = thingType;
	m_spriteFile = spriteFile;
	refreshAll();
}

void ThingPropertyEditor::clear()
{
	m_thingType = nullptr;
	m_spriteFile = nullptr;
	clearAllFields();
	m_animationPreview->clear();
}

ThingType *ThingPropertyEditor::thingType() const { return m_thingType; }
const SpriteFile *ThingPropertyEditor::spriteFile() const { return m_spriteFile; }

void ThingPropertyEditor::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	m_propertyTabs = new QTabWidget;
	mainLayout->addWidget(m_propertyTabs);

	// Tab 1: Attributes
	auto *attrScroll = new QScrollArea;
	attrScroll->setWidgetResizable(true);
	attrScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *attrWidget = new QWidget;
	m_attrLayout = new QVBoxLayout(attrWidget);
	m_attrLayout->setContentsMargins(8, 8, 8, 8);
	m_attrLayout->setSpacing(6);

	buildAttributeSection();

	m_attrLayout->addStretch();
	attrScroll->setWidget(attrWidget);
	m_propertyTabs->addTab(attrScroll, "Attributes");

	// Tab 2: Sprites & Frame Groups
	auto *spriteScroll = new QScrollArea;
	spriteScroll->setWidgetResizable(true);
	spriteScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto *spriteWidget = new QWidget;
	m_spriteLayout = new QVBoxLayout(spriteWidget);
	m_spriteLayout->setContentsMargins(8, 8, 8, 8);
	m_spriteLayout->setSpacing(6);

	buildFrameGroupSection();

	m_spriteLayout->addStretch();
	spriteScroll->setWidget(spriteWidget);
	m_propertyTabs->addTab(spriteScroll, "Sprites");

	// Tab 3: Animation Preview
	m_animationPreview = new AnimationPreview;
	m_propertyTabs->addTab(m_animationPreview, "Animation");
}

// ============================================================================
// Attribute section
// ============================================================================

void ThingPropertyEditor::buildAttributeSection()
{
	// Info section
	auto *infoGroup = new QGroupBox("Thing Info");
	auto *infoLayout = new QGridLayout(infoGroup);
	infoLayout->setContentsMargins(8, 12, 8, 8);
	infoLayout->setSpacing(4);

	infoLayout->addWidget(new QLabel("ID:"), 0, 0);
	m_idLabel = new QLabel("-");
	m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	infoLayout->addWidget(m_idLabel, 0, 1);

	infoLayout->addWidget(new QLabel("Category:"), 1, 0);
	m_categoryLabel = new QLabel("-");
	infoLayout->addWidget(m_categoryLabel, 1, 1);

	m_attrLayout->addWidget(infoGroup);

	// Boolean flags group
	m_flagsGroup = new QGroupBox("Flags");
	auto *flagsLayout = new QGridLayout(m_flagsGroup);
	flagsLayout->setContentsMargins(8, 12, 8, 8);
	flagsLayout->setSpacing(4);

	struct BoolAttrDef
	{
			uint8_t attr;
			const char *label;
	};

	static const BoolAttrDef boolAttrs[] = {
	    {.attr = ThingAttrGroundBorder, .label = "Ground Border"},
	    {.attr = ThingAttrOnBottom, .label = "On Bottom"},
	    {.attr = ThingAttrOnTop, .label = "On Top"},
	    {.attr = ThingAttrContainer, .label = "Container"},
	    {.attr = ThingAttrStackable, .label = "Stackable"},
	    {.attr = ThingAttrForceUse, .label = "Force Use"},
	    {.attr = ThingAttrMultiUse, .label = "Multi Use"},
	    {.attr = ThingAttrFluidContainer, .label = "Fluid Container"},
	    {.attr = ThingAttrSplash, .label = "Splash"},
	    {.attr = ThingAttrNotWalkable, .label = "Not Walkable"},
	    {.attr = ThingAttrNotMoveable, .label = "Not Moveable"},
	    {.attr = ThingAttrBlockProjectile, .label = "Block Projectile"},
	    {.attr = ThingAttrNotPathable, .label = "Not Pathable"},
	    {.attr = ThingAttrNoMoveAnimation, .label = "No Move Anim"},
	    {.attr = ThingAttrPickupable, .label = "Pickupable"},
	    {.attr = ThingAttrHangable, .label = "Hangable"},
	    {.attr = ThingAttrHookSouth, .label = "Hook South"},
	    {.attr = ThingAttrHookEast, .label = "Hook East"},
	    {.attr = ThingAttrRotateable, .label = "Rotateable"},
	    {.attr = ThingAttrDontHide, .label = "Don't Hide"},
	    {.attr = ThingAttrTranslucent, .label = "Translucent"},
	    {.attr = ThingAttrLyingCorpse, .label = "Lying Corpse"},
	    {.attr = ThingAttrAnimateAlways, .label = "Animate Always"},
	    {.attr = ThingAttrFullGround, .label = "Full Ground"},
	    {.attr = ThingAttrLook, .label = "Ignore Look"},
	    {.attr = ThingAttrWrapable, .label = "Wrapable"},
	    {.attr = ThingAttrUnwrapable, .label = "Unwrapable"},
	    {.attr = ThingAttrTopEffect, .label = "Top Effect"},
	};

	constexpr int boolAttrCount = sizeof(boolAttrs) / sizeof(boolAttrs[0]);
	int flagRow = 0;
	int flagCol = 0;
	constexpr int flagColumns = 3;

	for (int i = 0; i < boolAttrCount; ++i) {
		auto *cb = new QCheckBox(boolAttrs[i].label);
		uint8_t attr = boolAttrs[i].attr;

		connect(cb, &QCheckBox::toggled, this, [this, attr](bool checked) {
			if (m_blockSignals || !m_thingType)
				return;
			if (checked) {
				m_thingType->setAttr(attr, true);
			} else {
				m_thingType->removeAttr(attr);
			}
			emit thingModified();
		});

		m_boolCheckboxes.push_back({attr, cb});
		flagsLayout->addWidget(cb, flagRow, flagCol);

		++flagCol;
		if (flagCol >= flagColumns) {
			flagCol = 0;
			++flagRow;
		}
	}

	m_attrLayout->addWidget(m_flagsGroup);

	// Value attributes group
	m_valuesGroup = new QGroupBox("Value Attributes");
	auto *valuesLayout = new QGridLayout(m_valuesGroup);
	valuesLayout->setContentsMargins(8, 12, 8, 8);
	valuesLayout->setSpacing(4);

	int valRow = 0;

	// Ground speed
	valuesLayout->addWidget(new QLabel("Ground Speed:"), valRow, 0);
	m_groundSpeedCheck = new QCheckBox;
	valuesLayout->addWidget(m_groundSpeedCheck, valRow, 1);
	m_groundSpeedSpin = new QSpinBox;
	m_groundSpeedSpin->setRange(0, 65535);
	m_groundSpeedSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_groundSpeedSpin, valRow, 2);
	connectU16Attr(m_groundSpeedCheck, m_groundSpeedSpin, ThingAttrGround);
	++valRow;

	// Writable max length
	valuesLayout->addWidget(new QLabel("Writable (max len):"), valRow, 0);
	m_writableCheck = new QCheckBox;
	valuesLayout->addWidget(m_writableCheck, valRow, 1);
	m_writableSpin = new QSpinBox;
	m_writableSpin->setRange(0, 65535);
	m_writableSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_writableSpin, valRow, 2);
	connectU16Attr(m_writableCheck, m_writableSpin, ThingAttrWritable);
	++valRow;

	// Writable once max length
	valuesLayout->addWidget(new QLabel("Writable Once (max len):"), valRow, 0);
	m_writableOnceCheck = new QCheckBox;
	valuesLayout->addWidget(m_writableOnceCheck, valRow, 1);
	m_writableOnceSpin = new QSpinBox;
	m_writableOnceSpin->setRange(0, 65535);
	m_writableOnceSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_writableOnceSpin, valRow, 2);
	connectU16Attr(m_writableOnceCheck, m_writableOnceSpin, ThingAttrWritableOnce);
	++valRow;

	// Minimap color
	valuesLayout->addWidget(new QLabel("Minimap Color:"), valRow, 0);
	m_minimapColorCheck = new QCheckBox;
	valuesLayout->addWidget(m_minimapColorCheck, valRow, 1);
	m_minimapColorSpin = new QSpinBox;
	m_minimapColorSpin->setRange(0, 65535);
	m_minimapColorSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_minimapColorSpin, valRow, 2);
	connectU16Attr(m_minimapColorCheck, m_minimapColorSpin, ThingAttrMinimapColor);
	++valRow;

	// Cloth slot
	valuesLayout->addWidget(new QLabel("Cloth Slot:"), valRow, 0);
	m_clothCheck = new QCheckBox;
	valuesLayout->addWidget(m_clothCheck, valRow, 1);
	m_clothSpin = new QSpinBox;
	m_clothSpin->setRange(0, 65535);
	m_clothSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_clothSpin, valRow, 2);
	connectU16Attr(m_clothCheck, m_clothSpin, ThingAttrCloth);
	++valRow;

	// Lens help
	valuesLayout->addWidget(new QLabel("Lens Help:"), valRow, 0);
	m_lensHelpCheck = new QCheckBox;
	valuesLayout->addWidget(m_lensHelpCheck, valRow, 1);
	m_lensHelpSpin = new QSpinBox;
	m_lensHelpSpin->setRange(0, 65535);
	m_lensHelpSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_lensHelpSpin, valRow, 2);
	connectU16Attr(m_lensHelpCheck, m_lensHelpSpin, ThingAttrLensHelp);
	++valRow;

	// Usable
	valuesLayout->addWidget(new QLabel("Usable:"), valRow, 0);
	m_usableCheck = new QCheckBox;
	valuesLayout->addWidget(m_usableCheck, valRow, 1);
	m_usableSpin = new QSpinBox;
	m_usableSpin->setRange(0, 65535);
	m_usableSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_usableSpin, valRow, 2);
	connectU16Attr(m_usableCheck, m_usableSpin, ThingAttrUsable);
	++valRow;

	// Elevation
	valuesLayout->addWidget(new QLabel("Elevation:"), valRow, 0);
	m_elevationCheck = new QCheckBox;
	valuesLayout->addWidget(m_elevationCheck, valRow, 1);
	m_elevationSpin = new QSpinBox;
	m_elevationSpin->setRange(0, 65535);
	m_elevationSpin->setFixedWidth(100);
	valuesLayout->addWidget(m_elevationSpin, valRow, 2);
	connectU16Attr(m_elevationCheck, m_elevationSpin, ThingAttrElevation);
	++valRow;

	m_attrLayout->addWidget(m_valuesGroup);

	// Light group
	auto *lightGroup = new QGroupBox("Light");
	auto *lightLayout = new QGridLayout(lightGroup);
	lightLayout->setContentsMargins(8, 12, 8, 8);
	lightLayout->setSpacing(4);

	m_lightCheck = new QCheckBox("Has Light");
	lightLayout->addWidget(m_lightCheck, 0, 0, 1, 2);

	lightLayout->addWidget(new QLabel("Intensity:"), 1, 0);
	m_lightIntensitySpin = new QSpinBox;
	m_lightIntensitySpin->setRange(0, 65535);
	m_lightIntensitySpin->setFixedWidth(100);
	lightLayout->addWidget(m_lightIntensitySpin, 1, 1);

	lightLayout->addWidget(new QLabel("Color:"), 2, 0);
	m_lightColorSpin = new QSpinBox;
	m_lightColorSpin->setRange(0, 65535);
	m_lightColorSpin->setFixedWidth(100);
	lightLayout->addWidget(m_lightColorSpin, 2, 1);

	connect(m_lightCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_blockSignals || !m_thingType)
			return;
		m_lightIntensitySpin->setEnabled(checked);
		m_lightColorSpin->setEnabled(checked);
		if (checked) {
			LightData light;
			light.intensity = static_cast<uint16_t>(m_lightIntensitySpin->value());
			light.color = static_cast<uint16_t>(m_lightColorSpin->value());
			m_thingType->setAttr(ThingAttrLight, light);
		} else {
			m_thingType->removeAttr(ThingAttrLight);
		}
		emit thingModified();
	});

	auto lightValueChanged = [this]() {
		if (m_blockSignals || !m_thingType || !m_lightCheck->isChecked())
			return;
		LightData light;
		light.intensity = static_cast<uint16_t>(m_lightIntensitySpin->value());
		light.color = static_cast<uint16_t>(m_lightColorSpin->value());
		m_thingType->setAttr(ThingAttrLight, light);
		emit thingModified();
	};

	connect(m_lightIntensitySpin, qOverload<int>(&QSpinBox::valueChanged), this, lightValueChanged);
	connect(m_lightColorSpin, qOverload<int>(&QSpinBox::valueChanged), this, lightValueChanged);

	m_attrLayout->addWidget(lightGroup);

	// Displacement group
	auto *dispGroup = new QGroupBox("Displacement");
	auto *dispLayout = new QGridLayout(dispGroup);
	dispLayout->setContentsMargins(8, 12, 8, 8);
	dispLayout->setSpacing(4);

	m_displacementCheck = new QCheckBox("Has Displacement");
	dispLayout->addWidget(m_displacementCheck, 0, 0, 1, 2);

	dispLayout->addWidget(new QLabel("X:"), 1, 0);
	m_displacementXSpin = new QSpinBox;
	m_displacementXSpin->setRange(0, 65535);
	m_displacementXSpin->setFixedWidth(100);
	dispLayout->addWidget(m_displacementXSpin, 1, 1);

	dispLayout->addWidget(new QLabel("Y:"), 2, 0);
	m_displacementYSpin = new QSpinBox;
	m_displacementYSpin->setRange(0, 65535);
	m_displacementYSpin->setFixedWidth(100);
	dispLayout->addWidget(m_displacementYSpin, 2, 1);

	connect(m_displacementCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_blockSignals || !m_thingType)
			return;
		m_displacementXSpin->setEnabled(checked);
		m_displacementYSpin->setEnabled(checked);
		if (checked) {
			DisplacementData disp;
			disp.x = static_cast<uint16_t>(m_displacementXSpin->value());
			disp.y = static_cast<uint16_t>(m_displacementYSpin->value());
			m_thingType->displacement = disp;
			m_thingType->setAttr(ThingAttrDisplacement, disp);
		} else {
			m_thingType->displacement = {};
			m_thingType->removeAttr(ThingAttrDisplacement);
		}
		emit thingModified();
	});

	auto dispValueChanged = [this]() {
		if (m_blockSignals || !m_thingType || !m_displacementCheck->isChecked())
			return;
		DisplacementData disp;
		disp.x = static_cast<uint16_t>(m_displacementXSpin->value());
		disp.y = static_cast<uint16_t>(m_displacementYSpin->value());
		m_thingType->displacement = disp;
		m_thingType->setAttr(ThingAttrDisplacement, disp);
		emit thingModified();
	};

	connect(m_displacementXSpin, qOverload<int>(&QSpinBox::valueChanged), this, dispValueChanged);
	connect(m_displacementYSpin, qOverload<int>(&QSpinBox::valueChanged), this, dispValueChanged);

	m_attrLayout->addWidget(dispGroup);

	// Market data group
	m_marketGroup = new QGroupBox("Market Data");
	auto *marketLayout = new QGridLayout(m_marketGroup);
	marketLayout->setContentsMargins(8, 12, 8, 8);
	marketLayout->setSpacing(4);

	m_marketCheck = new QCheckBox("Has Market Data");
	marketLayout->addWidget(m_marketCheck, 0, 0, 1, 2);

	marketLayout->addWidget(new QLabel("Name:"), 1, 0);
	m_marketNameEdit = new QLineEdit;
	marketLayout->addWidget(m_marketNameEdit, 1, 1);

	marketLayout->addWidget(new QLabel("Category:"), 2, 0);
	m_marketCategorySpin = new QSpinBox;
	m_marketCategorySpin->setRange(0, 65535);
	m_marketCategorySpin->setFixedWidth(100);
	marketLayout->addWidget(m_marketCategorySpin, 2, 1);

	marketLayout->addWidget(new QLabel("Trade As:"), 3, 0);
	m_marketTradeAsSpin = new QSpinBox;
	m_marketTradeAsSpin->setRange(0, 65535);
	m_marketTradeAsSpin->setFixedWidth(100);
	marketLayout->addWidget(m_marketTradeAsSpin, 3, 1);

	marketLayout->addWidget(new QLabel("Show As:"), 4, 0);
	m_marketShowAsSpin = new QSpinBox;
	m_marketShowAsSpin->setRange(0, 65535);
	m_marketShowAsSpin->setFixedWidth(100);
	marketLayout->addWidget(m_marketShowAsSpin, 4, 1);

	marketLayout->addWidget(new QLabel("Restrict Vocation:"), 5, 0);
	m_marketVocationSpin = new QSpinBox;
	m_marketVocationSpin->setRange(0, 65535);
	m_marketVocationSpin->setFixedWidth(100);
	marketLayout->addWidget(m_marketVocationSpin, 5, 1);

	marketLayout->addWidget(new QLabel("Required Level:"), 6, 0);
	m_marketLevelSpin = new QSpinBox;
	m_marketLevelSpin->setRange(0, 65535);
	m_marketLevelSpin->setFixedWidth(100);
	marketLayout->addWidget(m_marketLevelSpin, 6, 1);

	connect(m_marketCheck, &QCheckBox::toggled, this, [this](bool checked) {
		if (m_blockSignals || !m_thingType)
			return;
		setMarketFieldsEnabled(checked);
		if (checked) {
			syncMarketToThing();
		} else {
			m_thingType->removeAttr(ThingAttrMarket);
		}
		emit thingModified();
	});

	auto marketChanged = [this]() {
		if (m_blockSignals || !m_thingType || !m_marketCheck->isChecked())
			return;
		syncMarketToThing();
		emit thingModified();
	};

	connect(m_marketNameEdit, &QLineEdit::textChanged, this, marketChanged);
	connect(m_marketCategorySpin, qOverload<int>(&QSpinBox::valueChanged), this, marketChanged);
	connect(m_marketTradeAsSpin, qOverload<int>(&QSpinBox::valueChanged), this, marketChanged);
	connect(m_marketShowAsSpin, qOverload<int>(&QSpinBox::valueChanged), this, marketChanged);
	connect(m_marketVocationSpin, qOverload<int>(&QSpinBox::valueChanged), this, marketChanged);
	connect(m_marketLevelSpin, qOverload<int>(&QSpinBox::valueChanged), this, marketChanged);

	m_attrLayout->addWidget(m_marketGroup);
}

void ThingPropertyEditor::syncMarketToThing()
{
	if (!m_thingType)
		return;
	MarketData market;
	market.name = m_marketNameEdit->text().toStdString();
	market.category = static_cast<uint16_t>(m_marketCategorySpin->value());
	market.tradeAs = static_cast<uint16_t>(m_marketTradeAsSpin->value());
	market.showAs = static_cast<uint16_t>(m_marketShowAsSpin->value());
	market.restrictVocation = static_cast<uint16_t>(m_marketVocationSpin->value());
	market.requiredLevel = static_cast<uint16_t>(m_marketLevelSpin->value());
	m_thingType->setAttr(ThingAttrMarket, market);
}

void ThingPropertyEditor::setMarketFieldsEnabled(bool enabled)
{
	m_marketNameEdit->setEnabled(enabled);
	m_marketCategorySpin->setEnabled(enabled);
	m_marketTradeAsSpin->setEnabled(enabled);
	m_marketShowAsSpin->setEnabled(enabled);
	m_marketVocationSpin->setEnabled(enabled);
	m_marketLevelSpin->setEnabled(enabled);
}

void ThingPropertyEditor::connectU16Attr(QCheckBox *check, QSpinBox *spin, uint8_t attr)
{
	connect(check, &QCheckBox::toggled, this, [this, check, spin, attr](bool checked) {
		(void)check;
		if (m_blockSignals || !m_thingType)
			return;
		spin->setEnabled(checked);
		if (checked) {
			uint16_t val = static_cast<uint16_t>(spin->value());
			m_thingType->setAttr(attr, val);
			if (attr == ThingAttrElevation) {
				m_thingType->elevation = val;
			}
		} else {
			m_thingType->removeAttr(attr);
			if (attr == ThingAttrElevation) {
				m_thingType->elevation = 0;
			}
		}
		emit thingModified();
	});

	connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, check, attr](int value) {
		if (m_blockSignals || !m_thingType || !check->isChecked())
			return;
		uint16_t val = static_cast<uint16_t>(value);
		m_thingType->setAttr(attr, val);
		if (attr == ThingAttrElevation) {
			m_thingType->elevation = val;
		}
		emit thingModified();
	});
}

// ============================================================================
// Frame group / sprite assignment section
// ============================================================================

void ThingPropertyEditor::buildFrameGroupSection()
{
	// Frame group selector
	auto *fgSelectorGroup = new QGroupBox("Frame Group");
	auto *fgSelectorLayout = new QHBoxLayout(fgSelectorGroup);
	fgSelectorLayout->setContentsMargins(8, 12, 8, 8);
	fgSelectorLayout->setSpacing(6);

	fgSelectorLayout->addWidget(new QLabel("Group:"));
	m_frameGroupCombo = new QComboBox;
	m_frameGroupCombo->addItem("Default / Idle", static_cast<int>(FrameGroupType::Default));
	m_frameGroupCombo->addItem("Walking", static_cast<int>(FrameGroupType::Walking));
	m_frameGroupCombo->setFixedWidth(160);
	connect(m_frameGroupCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ThingPropertyEditor::onFrameGroupChanged);
	fgSelectorLayout->addWidget(m_frameGroupCombo);

	m_addFrameGroupButton = new QPushButton("Add Group");
	m_addFrameGroupButton->setFixedWidth(90);
	connect(m_addFrameGroupButton, &QPushButton::clicked, this, &ThingPropertyEditor::onAddFrameGroupClicked);
	fgSelectorLayout->addWidget(m_addFrameGroupButton);

	m_removeFrameGroupButton = new QPushButton("Remove Group");
	m_removeFrameGroupButton->setFixedWidth(100);
	connect(m_removeFrameGroupButton, &QPushButton::clicked, this, &ThingPropertyEditor::onRemoveFrameGroupClicked);
	fgSelectorLayout->addWidget(m_removeFrameGroupButton);

	fgSelectorLayout->addStretch();
	m_spriteLayout->addWidget(fgSelectorGroup);

	// Frame group properties
	auto *fgPropsGroup = new QGroupBox("Frame Group Properties");
	auto *fgPropsLayout = new QGridLayout(fgPropsGroup);
	fgPropsLayout->setContentsMargins(8, 12, 8, 8);
	fgPropsLayout->setSpacing(4);

	int row = 0;

	fgPropsLayout->addWidget(new QLabel("Width (tiles):"), row, 0);
	m_fgWidthSpin = new QSpinBox;
	m_fgWidthSpin->setRange(1, 8);
	m_fgWidthSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgWidthSpin, row, 1);

	fgPropsLayout->addWidget(new QLabel("Height (tiles):"), row, 2);
	m_fgHeightSpin = new QSpinBox;
	m_fgHeightSpin->setRange(1, 8);
	m_fgHeightSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgHeightSpin, row, 3);
	++row;

	fgPropsLayout->addWidget(new QLabel("Exact Size:"), row, 0);
	m_fgExactSizeSpin = new QSpinBox;
	m_fgExactSizeSpin->setRange(1, 255);
	m_fgExactSizeSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgExactSizeSpin, row, 1);

	fgPropsLayout->addWidget(new QLabel("Layers:"), row, 2);
	m_fgLayersSpin = new QSpinBox;
	m_fgLayersSpin->setRange(1, 8);
	m_fgLayersSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgLayersSpin, row, 3);
	++row;

	fgPropsLayout->addWidget(new QLabel("Pattern X:"), row, 0);
	m_fgPatternXSpin = new QSpinBox;
	m_fgPatternXSpin->setRange(1, 16);
	m_fgPatternXSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgPatternXSpin, row, 1);

	fgPropsLayout->addWidget(new QLabel("Pattern Y:"), row, 2);
	m_fgPatternYSpin = new QSpinBox;
	m_fgPatternYSpin->setRange(1, 16);
	m_fgPatternYSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgPatternYSpin, row, 3);
	++row;

	fgPropsLayout->addWidget(new QLabel("Pattern Z:"), row, 0);
	m_fgPatternZSpin = new QSpinBox;
	m_fgPatternZSpin->setRange(1, 16);
	m_fgPatternZSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgPatternZSpin, row, 1);

	fgPropsLayout->addWidget(new QLabel("Anim Phases:"), row, 2);
	m_fgAnimPhasesSpin = new QSpinBox;
	m_fgAnimPhasesSpin->setRange(1, 255);
	m_fgAnimPhasesSpin->setFixedWidth(70);
	fgPropsLayout->addWidget(m_fgAnimPhasesSpin, row, 3);
	++row;

	m_fgApplyPropsButton = new QPushButton("Apply Properties (Resizes Sprite List)");
	fgPropsLayout->addWidget(m_fgApplyPropsButton, row, 0, 1, 4);
	connect(m_fgApplyPropsButton, &QPushButton::clicked, this, &ThingPropertyEditor::onApplyFrameGroupProps);

	m_spriteLayout->addWidget(fgPropsGroup);

	// Animation data
	auto *animGroup = new QGroupBox("Animation Data");
	auto *animLayout = new QGridLayout(animGroup);
	animLayout->setContentsMargins(8, 12, 8, 8);
	animLayout->setSpacing(4);

	m_animEnabledLabel = new QLabel("(Animation is enabled when phases > 1)");
	m_animEnabledLabel->setStyleSheet("color: #666666;");
	animLayout->addWidget(m_animEnabledLabel, 0, 0, 1, 4);

	animLayout->addWidget(new QLabel("Async:"), 1, 0);
	m_animAsyncCheck = new QCheckBox;
	animLayout->addWidget(m_animAsyncCheck, 1, 1);

	animLayout->addWidget(new QLabel("Loop Count:"), 1, 2);
	m_animLoopCountSpin = new QSpinBox;
	m_animLoopCountSpin->setRange(-1, 100000);
	m_animLoopCountSpin->setFixedWidth(80);
	animLayout->addWidget(m_animLoopCountSpin, 1, 3);

	animLayout->addWidget(new QLabel("Start Phase:"), 2, 0);
	m_animStartPhaseSpin = new QSpinBox;
	m_animStartPhaseSpin->setRange(-1, 254);
	m_animStartPhaseSpin->setFixedWidth(80);
	animLayout->addWidget(m_animStartPhaseSpin, 2, 1);

	// Phase durations table
	m_phaseDurationTable = new QTableWidget;
	m_phaseDurationTable->setColumnCount(3);
	m_phaseDurationTable->setHorizontalHeaderLabels({"Phase", "Min (ms)", "Max (ms)"});
	m_phaseDurationTable->horizontalHeader()->setStretchLastSection(true);
	m_phaseDurationTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_phaseDurationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_phaseDurationTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_phaseDurationTable->setMaximumHeight(200);
	animLayout->addWidget(m_phaseDurationTable, 3, 0, 1, 4);

	m_applyAnimButton = new QPushButton("Apply Animation Data");
	animLayout->addWidget(m_applyAnimButton, 4, 0, 1, 4);
	connect(m_applyAnimButton, &QPushButton::clicked, this, &ThingPropertyEditor::onApplyAnimationData);

	m_spriteLayout->addWidget(animGroup);

	// Sprite IDs table
	auto *spriteIdsGroup = new QGroupBox("Sprite IDs");
	auto *spriteIdsLayout = new QVBoxLayout(spriteIdsGroup);
	spriteIdsLayout->setContentsMargins(8, 12, 8, 8);
	spriteIdsLayout->setSpacing(4);

	m_spriteIdsTable = new QTableWidget;
	m_spriteIdsTable->setColumnCount(3);
	m_spriteIdsTable->setHorizontalHeaderLabels({"Index", "Sprite ID", "Preview"});
	m_spriteIdsTable->horizontalHeader()->setStretchLastSection(true);
	m_spriteIdsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_spriteIdsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
	m_spriteIdsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_spriteIdsTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_spriteIdsTable->verticalHeader()->setDefaultSectionSize(36);
	m_spriteIdsTable->setMaximumHeight(300);
	spriteIdsLayout->addWidget(m_spriteIdsTable);

	auto *spriteIdsBtnLayout = new QHBoxLayout;
	spriteIdsBtnLayout->setSpacing(4);

	m_changeSpriteIdButton = new QPushButton("Change ID");
	m_changeSpriteIdButton->setFixedWidth(90);
	connect(m_changeSpriteIdButton, &QPushButton::clicked, this, &ThingPropertyEditor::onChangeSpriteIdClicked);
	spriteIdsBtnLayout->addWidget(m_changeSpriteIdButton);

	m_clearSpriteIdButton = new QPushButton("Clear");
	m_clearSpriteIdButton->setFixedWidth(60);
	connect(m_clearSpriteIdButton, &QPushButton::clicked, this, &ThingPropertyEditor::onClearSpriteIdClicked);
	spriteIdsBtnLayout->addWidget(m_clearSpriteIdButton);

	spriteIdsBtnLayout->addStretch();
	spriteIdsLayout->addLayout(spriteIdsBtnLayout);

	m_spriteLayout->addWidget(spriteIdsGroup);
}

// ============================================================================
// Refresh / populate
// ============================================================================

void ThingPropertyEditor::updateCategoryVisibility()
{
	bool isItem = m_thingType && m_thingType->category == ThingCategory::Item;
	m_flagsGroup->setVisible(isItem);
	m_valuesGroup->setVisible(isItem);
	m_marketGroup->setVisible(isItem);
}

void ThingPropertyEditor::refreshAll()
{
	m_blockSignals = true;

	if (!m_thingType) {
		clearAllFields();
		m_blockSignals = false;
		m_animationPreview->clear();
		return;
	}

	// Info
	m_idLabel->setText(QString::number(m_thingType->id));
	m_categoryLabel->setText(QString::fromUtf8(thingCategoryName(m_thingType->category)));

	// Show/hide item-only groups based on category
	updateCategoryVisibility();

	// Boolean flags
	for (auto &[attr, cb]: m_boolCheckboxes) {
		cb->setChecked(m_thingType->hasAttr(attr));
	}

	// U16 attributes
	refreshU16Attr(m_groundSpeedCheck, m_groundSpeedSpin, ThingAttrGround);
	refreshU16Attr(m_writableCheck, m_writableSpin, ThingAttrWritable);
	refreshU16Attr(m_writableOnceCheck, m_writableOnceSpin, ThingAttrWritableOnce);
	refreshU16Attr(m_minimapColorCheck, m_minimapColorSpin, ThingAttrMinimapColor);
	refreshU16Attr(m_clothCheck, m_clothSpin, ThingAttrCloth);
	refreshU16Attr(m_lensHelpCheck, m_lensHelpSpin, ThingAttrLensHelp);
	refreshU16Attr(m_usableCheck, m_usableSpin, ThingAttrUsable);
	refreshU16Attr(m_elevationCheck, m_elevationSpin, ThingAttrElevation);

	// Light
	bool hasLight = m_thingType->hasAttr(ThingAttrLight);
	m_lightCheck->setChecked(hasLight);
	m_lightIntensitySpin->setEnabled(hasLight);
	m_lightColorSpin->setEnabled(hasLight);
	if (hasLight) {
		LightData light = m_thingType->getLightAttr();
		m_lightIntensitySpin->setValue(light.intensity);
		m_lightColorSpin->setValue(light.color);
	} else {
		m_lightIntensitySpin->setValue(0);
		m_lightColorSpin->setValue(215);
	}

	// Displacement
	bool hasDisp = m_thingType->hasAttr(ThingAttrDisplacement);
	m_displacementCheck->setChecked(hasDisp);
	m_displacementXSpin->setEnabled(hasDisp);
	m_displacementYSpin->setEnabled(hasDisp);
	m_displacementXSpin->setValue(m_thingType->displacement.x);
	m_displacementYSpin->setValue(m_thingType->displacement.y);

	// Market
	bool hasMarket = m_thingType->hasAttr(ThingAttrMarket);
	m_marketCheck->setChecked(hasMarket);
	setMarketFieldsEnabled(hasMarket);
	if (hasMarket) {
		MarketData market = m_thingType->getMarketAttr();
		m_marketNameEdit->setText(QString::fromStdString(market.name));
		m_marketCategorySpin->setValue(market.category);
		m_marketTradeAsSpin->setValue(market.tradeAs);
		m_marketShowAsSpin->setValue(market.showAs);
		m_marketVocationSpin->setValue(market.restrictVocation);
		m_marketLevelSpin->setValue(market.requiredLevel);
	} else {
		m_marketNameEdit->clear();
		m_marketCategorySpin->setValue(0);
		m_marketTradeAsSpin->setValue(0);
		m_marketShowAsSpin->setValue(0);
		m_marketVocationSpin->setValue(0);
		m_marketLevelSpin->setValue(0);
	}

	// Frame groups
	refreshFrameGroupSection();

	m_blockSignals = false;

	// Update animation preview
	if (m_spriteFile) {
		FrameGroupType fgType = currentFrameGroupType();
		m_animationPreview->setThingType(m_thingType, m_spriteFile);
		m_animationPreview->setFrameGroupType(fgType);
	} else {
		m_animationPreview->clear();
	}
}

void ThingPropertyEditor::refreshU16Attr(QCheckBox *check, QSpinBox *spin, uint8_t attr)
{
	bool has = m_thingType && m_thingType->hasAttr(attr);
	check->setChecked(has);
	spin->setEnabled(has);
	if (has) {
		spin->setValue(m_thingType->getU16Attr(attr));
	} else {
		spin->setValue(0);
	}
}

void ThingPropertyEditor::refreshFrameGroupSection()
{
	if (!m_thingType) {
		clearFrameGroupFields();
		return;
	}

	FrameGroupType fgType = currentFrameGroupType();
	const FrameGroup *group = m_thingType->getFrameGroup(fgType);

	bool hasGroup = (group != nullptr);

	// Check if the exact selected group exists (not falling back)
	int idx = static_cast<int>(fgType);
	bool exactGroupExists = (idx >= 0 && idx < FRAME_GROUP_COUNT && m_thingType->frameGroups[idx] != nullptr);

	m_addFrameGroupButton->setEnabled(!exactGroupExists);
	m_removeFrameGroupButton->setEnabled(exactGroupExists);

	if (!hasGroup) {
		clearFrameGroupFields();
		return;
	}

	m_fgWidthSpin->setValue(group->width);
	m_fgHeightSpin->setValue(group->height);
	m_fgExactSizeSpin->setValue(group->exactSize);
	m_fgLayersSpin->setValue(group->layers);
	m_fgPatternXSpin->setValue(group->patternX);
	m_fgPatternYSpin->setValue(group->patternY);
	m_fgPatternZSpin->setValue(group->patternZ);
	m_fgAnimPhasesSpin->setValue(group->animationPhases);

	// Animation data
	bool hasAnim = group->isAnimation && group->animator != nullptr;
	m_animAsyncCheck->setEnabled(hasAnim);
	m_animLoopCountSpin->setEnabled(hasAnim);
	m_animStartPhaseSpin->setEnabled(hasAnim);
	m_phaseDurationTable->setEnabled(hasAnim);
	m_applyAnimButton->setEnabled(hasAnim);

	if (hasAnim) {
		m_animEnabledLabel->setText("Animation is enabled.");
		m_animAsyncCheck->setChecked(group->animator->async);
		m_animLoopCountSpin->setValue(group->animator->loopCount);
		m_animStartPhaseSpin->setValue(group->animator->startPhase);

		// Fill phase durations
		const auto &durations = group->animator->phaseDurations;
		m_phaseDurationTable->setRowCount(static_cast<int>(durations.size()));
		for (int i = 0; i < static_cast<int>(durations.size()); ++i) {
			auto [minDur, maxDur] = durations[i];

			auto *phaseItem = new QTableWidgetItem(QString::number(i));
			phaseItem->setFlags(phaseItem->flags() & ~Qt::ItemIsEditable);
			m_phaseDurationTable->setItem(i, 0, phaseItem);

			auto *minItem = new QTableWidgetItem(QString::number(minDur));
			m_phaseDurationTable->setItem(i, 1, minItem);

			auto *maxItem = new QTableWidgetItem(QString::number(maxDur));
			m_phaseDurationTable->setItem(i, 2, maxItem);
		}
	} else {
		m_animEnabledLabel->setText("(Animation is enabled when phases > 1)");
		m_animAsyncCheck->setChecked(false);
		m_animLoopCountSpin->setValue(0);
		m_animStartPhaseSpin->setValue(-1);
		m_phaseDurationTable->setRowCount(0);
	}

	// Sprite IDs
	refreshSpriteIdsTable(group);
}

void ThingPropertyEditor::refreshSpriteIdsTable(const FrameGroup *group)
{
	m_spriteIdsTable->setRowCount(0);

	if (!group)
		return;

	const auto &spriteIds = group->spriteIds;
	m_spriteIdsTable->setRowCount(static_cast<int>(spriteIds.size()));

	for (int i = 0; i < static_cast<int>(spriteIds.size()); ++i) {
		uint32_t sprId = spriteIds[i];

		auto *indexItem = new QTableWidgetItem(QString::number(i));
		indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
		m_spriteIdsTable->setItem(i, 0, indexItem);

		auto *idItem = new QTableWidgetItem(QString::number(sprId));
		idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
		m_spriteIdsTable->setItem(i, 1, idItem);

		// Small preview
		if (m_spriteFile && sprId > 0) {
			QImage img = m_spriteFile->getSpriteImage(sprId);
			if (!img.isNull()) {
				QImage scaled = img.scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation);
				auto *previewItem = new QTableWidgetItem;
				previewItem->setData(Qt::DecorationRole, QPixmap::fromImage(scaled));
				previewItem->setFlags(previewItem->flags() & ~Qt::ItemIsEditable);
				m_spriteIdsTable->setItem(i, 2, previewItem);
			} else {
				auto *previewItem = new QTableWidgetItem("(empty)");
				previewItem->setFlags(previewItem->flags() & ~Qt::ItemIsEditable);
				m_spriteIdsTable->setItem(i, 2, previewItem);
			}
		} else {
			auto *previewItem = new QTableWidgetItem(sprId == 0 ? "(none)" : "(no spr file)");
			previewItem->setFlags(previewItem->flags() & ~Qt::ItemIsEditable);
			m_spriteIdsTable->setItem(i, 2, previewItem);
		}
	}
}

void ThingPropertyEditor::clearAllFields()
{
	m_blockSignals = true;

	m_idLabel->setText("-");
	m_categoryLabel->setText("-");

	// Reset item-only groups to visible when no thing is selected
	updateCategoryVisibility();

	for (auto &[attr, cb]: m_boolCheckboxes) {
		cb->setChecked(false);
	}

	m_groundSpeedCheck->setChecked(false);
	m_groundSpeedSpin->setValue(0);
	m_groundSpeedSpin->setEnabled(false);

	m_writableCheck->setChecked(false);
	m_writableSpin->setValue(0);
	m_writableSpin->setEnabled(false);

	m_writableOnceCheck->setChecked(false);
	m_writableOnceSpin->setValue(0);
	m_writableOnceSpin->setEnabled(false);

	m_minimapColorCheck->setChecked(false);
	m_minimapColorSpin->setValue(0);
	m_minimapColorSpin->setEnabled(false);

	m_clothCheck->setChecked(false);
	m_clothSpin->setValue(0);
	m_clothSpin->setEnabled(false);

	m_lensHelpCheck->setChecked(false);
	m_lensHelpSpin->setValue(0);
	m_lensHelpSpin->setEnabled(false);

	m_usableCheck->setChecked(false);
	m_usableSpin->setValue(0);
	m_usableSpin->setEnabled(false);

	m_elevationCheck->setChecked(false);
	m_elevationSpin->setValue(0);
	m_elevationSpin->setEnabled(false);

	m_lightCheck->setChecked(false);
	m_lightIntensitySpin->setValue(0);
	m_lightIntensitySpin->setEnabled(false);
	m_lightColorSpin->setValue(215);
	m_lightColorSpin->setEnabled(false);

	m_displacementCheck->setChecked(false);
	m_displacementXSpin->setValue(0);
	m_displacementXSpin->setEnabled(false);
	m_displacementYSpin->setValue(0);
	m_displacementYSpin->setEnabled(false);

	m_marketCheck->setChecked(false);
	setMarketFieldsEnabled(false);
	m_marketNameEdit->clear();
	m_marketCategorySpin->setValue(0);
	m_marketTradeAsSpin->setValue(0);
	m_marketShowAsSpin->setValue(0);
	m_marketVocationSpin->setValue(0);
	m_marketLevelSpin->setValue(0);

	clearFrameGroupFields();

	m_blockSignals = false;
}

void ThingPropertyEditor::clearFrameGroupFields()
{
	m_fgWidthSpin->setValue(1);
	m_fgHeightSpin->setValue(1);
	m_fgExactSizeSpin->setValue(32);
	m_fgLayersSpin->setValue(1);
	m_fgPatternXSpin->setValue(1);
	m_fgPatternYSpin->setValue(1);
	m_fgPatternZSpin->setValue(1);
	m_fgAnimPhasesSpin->setValue(1);

	m_animAsyncCheck->setChecked(false);
	m_animAsyncCheck->setEnabled(false);
	m_animLoopCountSpin->setValue(0);
	m_animLoopCountSpin->setEnabled(false);
	m_animStartPhaseSpin->setValue(-1);
	m_animStartPhaseSpin->setEnabled(false);
	m_phaseDurationTable->setRowCount(0);
	m_phaseDurationTable->setEnabled(false);
	m_applyAnimButton->setEnabled(false);
	m_animEnabledLabel->setText("(Animation is enabled when phases > 1)");

	m_spriteIdsTable->setRowCount(0);
}

FrameGroupType ThingPropertyEditor::currentFrameGroupType() const
{
	int idx = m_frameGroupCombo->currentIndex();
	if (idx < 0)
		return FrameGroupType::Default;
	return static_cast<FrameGroupType>(m_frameGroupCombo->currentData().toInt());
}

// ============================================================================
// Slots for frame group operations
// ============================================================================

void ThingPropertyEditor::onFrameGroupChanged()
{
	if (m_blockSignals)
		return;
	m_blockSignals = true;
	refreshFrameGroupSection();
	m_blockSignals = false;

	if (m_thingType && m_spriteFile) {
		FrameGroupType fgType = currentFrameGroupType();
		m_animationPreview->setThingType(m_thingType, m_spriteFile);
		m_animationPreview->setFrameGroupType(fgType);
	}
}

void ThingPropertyEditor::onAddFrameGroupClicked()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT)
		return;

	if (m_thingType->frameGroups[idx]) {
		QMessageBox::information(this, "Frame Group", "This frame group already exists.");
		return;
	}

	auto group = std::make_unique<FrameGroup>();
	group->type = fgType;
	group->width = 1;
	group->height = 1;
	group->exactSize = 32;
	group->layers = 1;
	group->patternX = 1;
	group->patternY = 1;
	group->patternZ = 1;
	group->animationPhases = 1;
	group->spriteIds.resize(1, 0);

	m_thingType->frameGroups[idx] = std::move(group);

	m_blockSignals = true;
	refreshFrameGroupSection();
	m_blockSignals = false;

	emit thingSpritesModified();
	emit thingModified();
}

void ThingPropertyEditor::onRemoveFrameGroupClicked()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT)
		return;

	if (!m_thingType->frameGroups[idx]) {
		return;
	}

	// Ensure at least one frame group remains
	int groupCount = 0;
	for (int i = 0; i < FRAME_GROUP_COUNT; ++i) {
		if (m_thingType->frameGroups[i])
			++groupCount;
	}

	if (groupCount <= 1) {
		QMessageBox::warning(this, "Frame Group", "Cannot remove the last frame group.");
		return;
	}

	QMessageBox::StandardButton result = QMessageBox::question(this, "Remove Frame Group", "Are you sure you want to remove this frame group?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (result != QMessageBox::Yes)
		return;

	m_thingType->frameGroups[idx].reset();

	m_blockSignals = true;
	refreshFrameGroupSection();
	m_blockSignals = false;

	emit thingSpritesModified();
	emit thingModified();
}

void ThingPropertyEditor::onApplyFrameGroupProps()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT || !m_thingType->frameGroups[idx]) {
		QMessageBox::warning(this, "Frame Group", "No frame group selected.");
		return;
	}

	FrameGroup *group = m_thingType->frameGroups[idx].get();

	uint8_t newWidth = static_cast<uint8_t>(m_fgWidthSpin->value());
	uint8_t newHeight = static_cast<uint8_t>(m_fgHeightSpin->value());
	uint8_t newExactSize = static_cast<uint8_t>(m_fgExactSizeSpin->value());
	uint8_t newLayers = static_cast<uint8_t>(m_fgLayersSpin->value());
	uint8_t newPatternX = static_cast<uint8_t>(m_fgPatternXSpin->value());
	uint8_t newPatternY = static_cast<uint8_t>(m_fgPatternYSpin->value());
	uint8_t newPatternZ = static_cast<uint8_t>(m_fgPatternZSpin->value());
	uint8_t newAnimPhases = static_cast<uint8_t>(m_fgAnimPhasesSpin->value());

	int newTotalSprites = static_cast<int>(newWidth) * newHeight * newLayers * newPatternX * newPatternY * newPatternZ * newAnimPhases;

	if (newTotalSprites > 4096) {
		QMessageBox::warning(this, "Frame Group", QString("Total sprite count would be %1, which exceeds the maximum of 4096.\n"
		                                                  "Please reduce the dimensions.")
		                                              .arg(newTotalSprites));
		return;
	}

	group->width = newWidth;
	group->height = newHeight;
	group->exactSize = newExactSize;
	group->layers = newLayers;
	group->patternX = newPatternX;
	group->patternY = newPatternY;
	group->patternZ = newPatternZ;
	group->animationPhases = newAnimPhases;

	// Resize sprite IDs array, preserving existing data where possible
	group->spriteIds.resize(static_cast<size_t>(newTotalSprites), 0);

	// Handle animation
	if (newAnimPhases > 1) {
		group->isAnimation = true;
		if (!group->animator) {
			group->animator = std::make_unique<AnimatorData>();
			group->animator->async = true;
			group->animator->loopCount = 0;
			group->animator->startPhase = -1;
		}
		// Resize phase durations
		while (static_cast<int>(group->animator->phaseDurations.size()) < newAnimPhases) {
			group->animator->phaseDurations.emplace_back(100, 100);
		}
		while (static_cast<int>(group->animator->phaseDurations.size()) > newAnimPhases) {
			group->animator->phaseDurations.pop_back();
		}
	} else {
		group->isAnimation = false;
		group->animator.reset();
	}

	m_blockSignals = true;
	refreshFrameGroupSection();
	m_blockSignals = false;

	// Update animation preview
	if (m_spriteFile) {
		m_animationPreview->setThingType(m_thingType, m_spriteFile);
		m_animationPreview->setFrameGroupType(fgType);
	}

	emit thingSpritesModified();
	emit thingModified();
}

void ThingPropertyEditor::onApplyAnimationData()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT || !m_thingType->frameGroups[idx])
		return;

	FrameGroup *group = m_thingType->frameGroups[idx].get();
	if (!group->animator)
		return;

	group->animator->async = m_animAsyncCheck->isChecked();
	group->animator->loopCount = m_animLoopCountSpin->value();
	group->animator->startPhase = static_cast<int8_t>(m_animStartPhaseSpin->value());

	// Read phase durations from table
	int rowCount = m_phaseDurationTable->rowCount();
	group->animator->phaseDurations.resize(static_cast<size_t>(rowCount));

	for (int i = 0; i < rowCount; ++i) {
		uint32_t minDur = 100;
		uint32_t maxDur = 100;

		QTableWidgetItem *minItem = m_phaseDurationTable->item(i, 1);
		if (minItem) {
			bool ok = false;
			uint32_t val = minItem->text().toUInt(&ok);
			if (ok)
				minDur = val;
		}

		QTableWidgetItem *maxItem = m_phaseDurationTable->item(i, 2);
		if (maxItem) {
			bool ok = false;
			uint32_t val = maxItem->text().toUInt(&ok);
			if (ok)
				maxDur = val;
		}

		group->animator->phaseDurations[i] = {minDur, maxDur};
	}

	// Update animation preview
	if (m_spriteFile) {
		m_animationPreview->setThingType(m_thingType, m_spriteFile);
		m_animationPreview->setFrameGroupType(fgType);
	}

	emit thingModified();
}

void ThingPropertyEditor::onChangeSpriteIdClicked()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT || !m_thingType->frameGroups[idx])
		return;

	FrameGroup *group = m_thingType->frameGroups[idx].get();

	int selectedRow = m_spriteIdsTable->currentRow();
	if (selectedRow < 0 || selectedRow >= static_cast<int>(group->spriteIds.size())) {
		QMessageBox::information(this, "Change Sprite ID", "Please select a sprite index in the table.");
		return;
	}

	uint32_t currentId = group->spriteIds[selectedRow];
	bool ok = false;
	int newId = QInputDialog::getInt(this, "Change Sprite ID", QString("Enter new sprite ID for index %1:").arg(selectedRow), static_cast<int>(currentId), 0, 999999999, 1, &ok);

	if (ok) {
		group->spriteIds[selectedRow] = static_cast<uint32_t>(newId);

		m_blockSignals = true;
		refreshSpriteIdsTable(group);
		m_blockSignals = false;

		// Refresh animation preview
		if (m_spriteFile) {
			m_animationPreview->setThingType(m_thingType, m_spriteFile);
			m_animationPreview->setFrameGroupType(fgType);
		}

		emit thingSpritesModified();
		emit thingModified();
	}
}

void ThingPropertyEditor::onClearSpriteIdClicked()
{
	if (!m_thingType)
		return;

	FrameGroupType fgType = currentFrameGroupType();
	int idx = static_cast<int>(fgType);
	if (idx < 0 || idx >= FRAME_GROUP_COUNT || !m_thingType->frameGroups[idx])
		return;

	FrameGroup *group = m_thingType->frameGroups[idx].get();

	int selectedRow = m_spriteIdsTable->currentRow();
	if (selectedRow < 0 || selectedRow >= static_cast<int>(group->spriteIds.size())) {
		QMessageBox::information(this, "Clear Sprite ID", "Please select a sprite index in the table.");
		return;
	}

	group->spriteIds[selectedRow] = 0;

	m_blockSignals = true;
	refreshSpriteIdsTable(group);
	m_blockSignals = false;

	if (m_spriteFile) {
		m_animationPreview->setThingType(m_thingType, m_spriteFile);
		m_animationPreview->setFrameGroupType(fgType);
	}

	emit thingSpritesModified();
	emit thingModified();
}
