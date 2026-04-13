/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/SpriteFile.h"
#include "core/ThingData.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <vector>

class QPainter;
class QVBoxLayout;
class QHBoxLayout;

class AnimationCanvas: public QWidget
{
		Q_OBJECT

	public:
		explicit AnimationCanvas(QWidget *parent = nullptr);
		~AnimationCanvas() override = default;

		void setFrame(const QImage &image);
		void clearFrame();

		void setScale(int scale);
		int scale() const;

	protected:
		void paintEvent(QPaintEvent *event) override;

	private:
		void drawCheckerboard(QPainter &painter, const QRect &area);

		QImage m_frame;
		QPixmap m_cachedPixmap;
		int m_scale = 2;
		bool m_pixmapDirty = true;
};

class AnimationPreview: public QWidget
{
		Q_OBJECT

	public:
		explicit AnimationPreview(QWidget *parent = nullptr);
		~AnimationPreview() override = default;

		void setThingType(const ThingType *thingType, const SpriteFile *spriteFile);
		void clear();
		void setFrameGroupType(FrameGroupType type);

		bool isPlaying() const;

	public slots:
		void play();
		void pause();
		void stop();
		void togglePlayPause();

	signals:
		void frameChanged(int phase);

	private slots:
		void onSliderChanged(int value);
		void onPatternChanged();
		void onFrameTypeChanged(int index);

	private:
		void setupUi();
		void rebuildFrameList();
		void updateFrameImages();
		void updatePatternLimits();
		void displayCurrentFrame();
		int getPhaseDuration(int phase) const;
		void startTimerForPhase(int phase);
		void advanceFrame();

		void rebuildPatternControls();
		void buildGenericPatternControls();
		void buildCreaturePatternControls();
		void clearPatternLayout();
		void updateAnimPlayerEnabled();
		void updateFrameTypeCombo();

		const ThingType *m_thingType = nullptr;
		const SpriteFile *m_spriteFile = nullptr;
		int m_currentPhase = 0;
		bool m_playing = false;
		bool m_looping = true;
		FrameGroupType m_currentFrameGroupType = FrameGroupType::Default;
		ThingCategory m_lastCategory = ThingCategory::Invalid;

		std::vector<QImage> m_frames;

		int m_patternX = 0;
		int m_patternY = 0;
		int m_patternZ = 0;
		int m_layer = 0;

		QTimer *m_timer = nullptr;

		// UI elements
		AnimationCanvas *m_canvas = nullptr;

		// Pattern GroupBox — contents rebuilt based on thing type
		QGroupBox *m_patternGroup = nullptr;
		QVBoxLayout *m_patternGroupInnerLayout = nullptr;
		QWidget *m_patternContent = nullptr;

		// Generic pattern controls (Missiles / Effects / Items)
		QSpinBox *m_patternXSpin = nullptr;
		QSpinBox *m_patternYSpin = nullptr;
		QSpinBox *m_patternZSpin = nullptr;
		QSpinBox *m_layerSpin = nullptr;

		// Creature-specific pattern controls
		QComboBox *m_directionCombo = nullptr;
		QComboBox *m_addonCombo = nullptr;
		QComboBox *m_stanceCombo = nullptr;
		QLabel *m_stanceLabel = nullptr;
		QCheckBox *m_showColorsCheck = nullptr;

		// Animation Player GroupBox
		QGroupBox *m_animPlayerGroup = nullptr;
		QPushButton *m_playButton = nullptr;
		QPushButton *m_stopButton = nullptr;
		QCheckBox *m_loopCheckBox = nullptr;
		QLabel *m_phaseLabel = nullptr;
		QSlider *m_phaseSlider = nullptr;

		// Frame Type ComboBox (inside Animation Player group)
		QComboBox *m_frameTypeCombo = nullptr;
		QLabel *m_frameTypeLabel = nullptr;
};
