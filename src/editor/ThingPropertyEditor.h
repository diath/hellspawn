/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/ThingData.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

#include <cstdint>
#include <utility>
#include <vector>

class SpriteFile;
class AnimationPreview;
class QGroupBox;
class QVBoxLayout;
class QTabWidget;

class ThingPropertyEditor: public QWidget
{
		Q_OBJECT

	public:
		explicit ThingPropertyEditor(QWidget *parent = nullptr);
		~ThingPropertyEditor() override = default;

		void setThingType(ThingType *thingType, const SpriteFile *spriteFile);
		void clear();

		ThingType *thingType() const;
		const SpriteFile *spriteFile() const;

	signals:
		void thingModified();
		void thingSpritesModified();

	private:
		void setupUi();

		void buildAttributeSection();
		void buildFrameGroupSection();

		void syncMarketToThing();
		void setMarketFieldsEnabled(bool enabled);
		void connectU16Attr(QCheckBox *check, QSpinBox *spin, uint8_t attr);

		void updateCategoryVisibility();
		void refreshAll();
		void refreshU16Attr(QCheckBox *check, QSpinBox *spin, uint8_t attr);
		void refreshFrameGroupSection();
		void refreshSpriteIdsTable(const FrameGroup *group);

		void clearAllFields();
		void clearFrameGroupFields();

		FrameGroupType currentFrameGroupType() const;

		void onFrameGroupChanged();
		void onAddFrameGroupClicked();
		void onRemoveFrameGroupClicked();
		void onApplyFrameGroupProps();
		void onApplyAnimationData();
		void onChangeSpriteIdClicked();
		void onClearSpriteIdClicked();

		ThingType *m_thingType = nullptr;
		const SpriteFile *m_spriteFile = nullptr;
		bool m_blockSignals = false;

		QTabWidget *m_propertyTabs = nullptr;
		QVBoxLayout *m_attrLayout = nullptr;
		QVBoxLayout *m_spriteLayout = nullptr;

		AnimationPreview *m_animationPreview = nullptr;

		// Info
		QLabel *m_idLabel = nullptr;
		QLabel *m_categoryLabel = nullptr;

		// Item-only groups (hidden for non-Item categories)
		QGroupBox *m_flagsGroup = nullptr;
		QGroupBox *m_valuesGroup = nullptr;
		QGroupBox *m_marketGroup = nullptr;

		// Boolean flags
		std::vector<std::pair<uint8_t, QCheckBox *>> m_boolCheckboxes;

		// U16 value attributes
		QCheckBox *m_groundSpeedCheck = nullptr;
		QSpinBox *m_groundSpeedSpin = nullptr;
		QCheckBox *m_writableCheck = nullptr;
		QSpinBox *m_writableSpin = nullptr;
		QCheckBox *m_writableOnceCheck = nullptr;
		QSpinBox *m_writableOnceSpin = nullptr;
		QCheckBox *m_minimapColorCheck = nullptr;
		QSpinBox *m_minimapColorSpin = nullptr;
		QCheckBox *m_clothCheck = nullptr;
		QSpinBox *m_clothSpin = nullptr;
		QCheckBox *m_lensHelpCheck = nullptr;
		QSpinBox *m_lensHelpSpin = nullptr;
		QCheckBox *m_usableCheck = nullptr;
		QSpinBox *m_usableSpin = nullptr;
		QCheckBox *m_elevationCheck = nullptr;
		QSpinBox *m_elevationSpin = nullptr;

		// Light
		QCheckBox *m_lightCheck = nullptr;
		QSpinBox *m_lightIntensitySpin = nullptr;
		QSpinBox *m_lightColorSpin = nullptr;

		// Displacement
		QCheckBox *m_displacementCheck = nullptr;
		QSpinBox *m_displacementXSpin = nullptr;
		QSpinBox *m_displacementYSpin = nullptr;

		// Market data
		QCheckBox *m_marketCheck = nullptr;
		QLineEdit *m_marketNameEdit = nullptr;
		QSpinBox *m_marketCategorySpin = nullptr;
		QSpinBox *m_marketTradeAsSpin = nullptr;
		QSpinBox *m_marketShowAsSpin = nullptr;
		QSpinBox *m_marketVocationSpin = nullptr;
		QSpinBox *m_marketLevelSpin = nullptr;

		// Frame group
		QComboBox *m_frameGroupCombo = nullptr;
		QPushButton *m_addFrameGroupButton = nullptr;
		QPushButton *m_removeFrameGroupButton = nullptr;

		QSpinBox *m_fgWidthSpin = nullptr;
		QSpinBox *m_fgHeightSpin = nullptr;
		QSpinBox *m_fgExactSizeSpin = nullptr;
		QSpinBox *m_fgLayersSpin = nullptr;
		QSpinBox *m_fgPatternXSpin = nullptr;
		QSpinBox *m_fgPatternYSpin = nullptr;
		QSpinBox *m_fgPatternZSpin = nullptr;
		QSpinBox *m_fgAnimPhasesSpin = nullptr;
		QPushButton *m_fgApplyPropsButton = nullptr;

		// Animation data
		QLabel *m_animEnabledLabel = nullptr;
		QCheckBox *m_animAsyncCheck = nullptr;
		QSpinBox *m_animLoopCountSpin = nullptr;
		QSpinBox *m_animStartPhaseSpin = nullptr;
		QTableWidget *m_phaseDurationTable = nullptr;
		QPushButton *m_applyAnimButton = nullptr;

		// Sprite IDs
		QTableWidget *m_spriteIdsTable = nullptr;
		QPushButton *m_changeSpriteIdButton = nullptr;
		QPushButton *m_clearSpriteIdButton = nullptr;
};
