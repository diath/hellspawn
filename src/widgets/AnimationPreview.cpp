/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "widgets/AnimationPreview.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

// =============================================================================
// AnimationCanvas implementation
// =============================================================================

AnimationCanvas::AnimationCanvas(QWidget *parent)
: QWidget(parent)
{
	setMinimumSize(68, 68);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setStyleSheet("background-color: #222222;");
}

void AnimationCanvas::setFrame(const QImage &image)
{
	m_frame = image;
	m_pixmapDirty = true;
	update();
}

void AnimationCanvas::clearFrame()
{
	m_frame = QImage();
	m_pixmapDirty = true;
	update();
}

void AnimationCanvas::setScale(int scale)
{
	if (scale < 1)
		scale = 1;
	if (scale > 8)
		scale = 8;
	m_scale = scale;
	m_pixmapDirty = true;
	update();
}

int AnimationCanvas::scale() const { return m_scale; }

void AnimationCanvas::paintEvent(QPaintEvent * /*event*/)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	// Draw checkerboard background in the center area
	int canvasSize = qMin(width(), height());
	int xOrigin = (width() - canvasSize) / 2;
	int yOrigin = (height() - canvasSize) / 2;
	QRect canvasRect(xOrigin, yOrigin, canvasSize, canvasSize);

	drawCheckerboard(painter, canvasRect);

	if (!m_frame.isNull()) {
		if (m_pixmapDirty) {
			QSize scaledSize(m_frame.width() * m_scale, m_frame.height() * m_scale);
			m_cachedPixmap = QPixmap::fromImage(
			    m_frame.scaled(scaledSize, Qt::KeepAspectRatio, Qt::FastTransformation)
			);
			m_pixmapDirty = false;
		}

		int xOff = xOrigin + (canvasSize - m_cachedPixmap.width()) / 2;
		int yOff = yOrigin + (canvasSize - m_cachedPixmap.height()) / 2;
		painter.drawPixmap(xOff, yOff, m_cachedPixmap);
	}

	// Border around canvas area
	painter.setPen(QPen(QColor(100, 100, 100), 1));
	painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));
}

void AnimationCanvas::drawCheckerboard(QPainter &painter, const QRect &area)
{
	const int cellSize = 8;
	QColor lightColor(80, 80, 80);
	QColor darkColor(60, 60, 60);

	for (int y = area.top(); y < area.bottom(); y += cellSize) {
		for (int x = area.left(); x < area.right(); x += cellSize) {
			int col = (x - area.left()) / cellSize;
			int row = (y - area.top()) / cellSize;
			bool light = ((col + row) % 2 == 0);
			QRect cellRect(x, y, qMin(cellSize, area.right() - x), qMin(cellSize, area.bottom() - y));
			painter.fillRect(cellRect, light ? lightColor : darkColor);
		}
	}
}

// =============================================================================
// AnimationPreview implementation
// =============================================================================

AnimationPreview::AnimationPreview(QWidget *parent)
: QWidget(parent)
{
	setupUi();

	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, &AnimationPreview::advanceFrame);
}

void AnimationPreview::setThingType(const ThingType *thingType, const SpriteFile *spriteFile)
{
	stop();
	m_thingType = thingType;
	m_spriteFile = spriteFile;
	m_currentPhase = 0;

	// Rebuild pattern controls if category changed
	ThingCategory newCategory = thingType ? thingType->category : ThingCategory::Invalid;
	if (newCategory != m_lastCategory) {
		m_lastCategory = newCategory;
		rebuildPatternControls();
	}

	rebuildFrameList();
	updatePatternLimits();
	updateAnimPlayerEnabled();
	updateFrameTypeCombo();
	displayCurrentFrame();
}

void AnimationPreview::clear()
{
	stop();
	m_thingType = nullptr;
	m_spriteFile = nullptr;
	m_frames.clear();
	m_currentPhase = 0;
	m_canvas->clearFrame();
	m_phaseLabel->setText("Frame: 0 / 0");
	m_phaseSlider->setRange(0, 0);
	m_phaseSlider->setValue(0);
	updateAnimPlayerEnabled();
}

void AnimationPreview::setFrameGroupType(FrameGroupType type)
{
	if (m_currentFrameGroupType != type) {
		m_currentFrameGroupType = type;
		stop();
		m_currentPhase = 0;
		rebuildFrameList();
		updatePatternLimits();
		updateAnimPlayerEnabled();
		updateFrameTypeCombo();
		displayCurrentFrame();
	}
}

bool AnimationPreview::isPlaying() const { return m_playing; }

void AnimationPreview::play()
{
	if (m_frames.empty())
		return;
	m_playing = true;
	m_playButton->setText("Pause");
	startTimerForPhase(m_currentPhase);
}

void AnimationPreview::pause()
{
	m_playing = false;
	m_timer->stop();
	m_playButton->setText("Play");
}

void AnimationPreview::stop()
{
	m_playing = false;
	m_timer->stop();
	m_currentPhase = 0;
	m_playButton->setText("Play");
	if (!m_frames.empty()) {
		m_phaseSlider->setValue(0);
	}
	displayCurrentFrame();
}

void AnimationPreview::togglePlayPause()
{
	if (m_playing) {
		pause();
	} else {
		play();
	}
}

void AnimationPreview::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(4);

	// Canvas for drawing the animation
	m_canvas = new AnimationCanvas;
	mainLayout->addWidget(m_canvas, 1);

	// =========================================================================
	// Pattern GroupBox — contents rebuilt dynamically based on thing type
	// =========================================================================
	m_patternGroup = new QGroupBox("Pattern");
	m_patternGroupInnerLayout = new QVBoxLayout(m_patternGroup);
	m_patternGroupInnerLayout->setContentsMargins(4, 4, 4, 4);
	m_patternGroupInnerLayout->setSpacing(4);

	// Build initial generic controls (default state)
	m_patternContent = nullptr;
	buildGenericPatternControls();

	mainLayout->addWidget(m_patternGroup);

	// =========================================================================
	// Animation Player GroupBox
	// =========================================================================
	m_animPlayerGroup = new QGroupBox("Animation Player");
	auto *animPlayerLayout = new QVBoxLayout(m_animPlayerGroup);
	animPlayerLayout->setContentsMargins(4, 4, 4, 4);
	animPlayerLayout->setSpacing(4);

	// Frame Type row (conditionally visible)
	auto *frameTypeLayout = new QHBoxLayout;
	frameTypeLayout->setContentsMargins(0, 0, 0, 0);
	frameTypeLayout->setSpacing(4);

	m_frameTypeLabel = new QLabel("Frame Type:");
	m_frameTypeCombo = new QComboBox;
	m_frameTypeCombo->setFixedWidth(100);
	connect(m_frameTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnimationPreview::onFrameTypeChanged);

	frameTypeLayout->addWidget(m_frameTypeLabel);
	frameTypeLayout->addWidget(m_frameTypeCombo);
	frameTypeLayout->addStretch();
	animPlayerLayout->addLayout(frameTypeLayout);

	// Initially hide frame type controls — shown only when walking frames exist
	m_frameTypeLabel->setVisible(false);
	m_frameTypeCombo->setVisible(false);

	// Playback controls row
	auto *controlsLayout = new QHBoxLayout;
	controlsLayout->setContentsMargins(0, 0, 0, 0);
	controlsLayout->setSpacing(4);

	m_playButton = new QPushButton("Play");
	m_playButton->setFixedWidth(65);
	connect(m_playButton, &QPushButton::clicked, this, &AnimationPreview::togglePlayPause);

	m_stopButton = new QPushButton("Stop");
	m_stopButton->setFixedWidth(50);
	connect(m_stopButton, &QPushButton::clicked, this, &AnimationPreview::stop);

	m_loopCheckBox = new QCheckBox("Loop");
	m_loopCheckBox->setChecked(true);
	connect(m_loopCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
		m_looping = checked;
	});

	m_phaseLabel = new QLabel("Frame: 0 / 0");

	controlsLayout->addWidget(m_playButton);
	controlsLayout->addWidget(m_stopButton);
	controlsLayout->addWidget(m_loopCheckBox);
	controlsLayout->addStretch();
	controlsLayout->addWidget(m_phaseLabel);

	animPlayerLayout->addLayout(controlsLayout);

	// Phase slider
	m_phaseSlider = new QSlider(Qt::Horizontal);
	m_phaseSlider->setMinimum(0);
	m_phaseSlider->setMaximum(0);
	m_phaseSlider->setSingleStep(1);
	m_phaseSlider->setPageStep(1);
	connect(m_phaseSlider, &QSlider::valueChanged, this, &AnimationPreview::onSliderChanged);

	animPlayerLayout->addWidget(m_phaseSlider);

	mainLayout->addWidget(m_animPlayerGroup);
}

// =============================================================================
// Pattern controls: rebuild based on thing category
// =============================================================================

void AnimationPreview::clearPatternLayout()
{
	// Remove the old pattern content widget from the inner layout and delete it
	if (m_patternContent) {
		m_patternGroupInnerLayout->removeWidget(m_patternContent);
		delete m_patternContent;
		m_patternContent = nullptr;
	}

	// Reset all widget pointers so we don't dangle
	m_patternXSpin = nullptr;
	m_patternYSpin = nullptr;
	m_patternZSpin = nullptr;
	m_layerSpin = nullptr;
	m_directionCombo = nullptr;
	m_addonCombo = nullptr;
	m_stanceCombo = nullptr;
	m_stanceLabel = nullptr;
	m_showColorsCheck = nullptr;
}

void AnimationPreview::rebuildPatternControls()
{
	clearPatternLayout();

	if (m_lastCategory == ThingCategory::Creature) {
		buildCreaturePatternControls();
	} else {
		buildGenericPatternControls();
	}

	// Sync internal pattern values from the freshly built controls.
	// Signal connections are established after widget construction, so
	// onPatternChanged() was never called during the build step.
	onPatternChanged();
}

void AnimationPreview::buildGenericPatternControls()
{
	m_patternContent = new QWidget;
	auto *layout = new QHBoxLayout(m_patternContent);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	layout->addWidget(new QLabel("X:"));
	m_patternXSpin = new QSpinBox;
	m_patternXSpin->setMinimum(0);
	m_patternXSpin->setMaximum(0);
	m_patternXSpin->setFixedWidth(55);
	connect(m_patternXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_patternXSpin);

	layout->addWidget(new QLabel("Y:"));
	m_patternYSpin = new QSpinBox;
	m_patternYSpin->setMinimum(0);
	m_patternYSpin->setMaximum(0);
	m_patternYSpin->setFixedWidth(55);
	connect(m_patternYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_patternYSpin);

	layout->addWidget(new QLabel("Z:"));
	m_patternZSpin = new QSpinBox;
	m_patternZSpin->setMinimum(0);
	m_patternZSpin->setMaximum(0);
	m_patternZSpin->setFixedWidth(55);
	connect(m_patternZSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_patternZSpin);

	layout->addWidget(new QLabel("Layer:"));
	m_layerSpin = new QSpinBox;
	m_layerSpin->setMinimum(0);
	m_layerSpin->setMaximum(0);
	m_layerSpin->setFixedWidth(55);
	connect(m_layerSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_layerSpin);

	layout->addStretch();
	m_patternGroupInnerLayout->addWidget(m_patternContent);
}

void AnimationPreview::buildCreaturePatternControls()
{
	m_patternContent = new QWidget;
	auto *layout = new QHBoxLayout(m_patternContent);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	// Direction ComboBox (replaces xPattern)
	layout->addWidget(new QLabel("Direction:"));
	m_directionCombo = new QComboBox;
	m_directionCombo->addItem("North", 0);
	m_directionCombo->addItem("East", 1);
	m_directionCombo->addItem("South", 2);
	m_directionCombo->addItem("West", 3);
	m_directionCombo->setCurrentIndex(2); // Default: South
	m_directionCombo->setFixedWidth(80);
	connect(m_directionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_directionCombo);

	// Addon ComboBox (replaces yPattern)
	layout->addWidget(new QLabel("Addon:"));
	m_addonCombo = new QComboBox;
	m_addonCombo->addItem("Base", 0);
	m_addonCombo->setFixedWidth(80);
	connect(m_addonCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_addonCombo);

	// Stance ComboBox (replaces zPattern)
	m_stanceLabel = new QLabel("Stance:");
	layout->addWidget(m_stanceLabel);
	m_stanceCombo = new QComboBox;
	m_stanceCombo->addItem("Normal", 0);
	m_stanceCombo->addItem("Mounted", 1);
	m_stanceCombo->setCurrentIndex(0);
	m_stanceCombo->setFixedWidth(80);
	connect(m_stanceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_stanceCombo);

	// Show Colors Checkbox (replaces layer spinbox)
	m_showColorsCheck = new QCheckBox("Show Colors");
	m_showColorsCheck->setChecked(false);
	connect(m_showColorsCheck, &QCheckBox::toggled, this, &AnimationPreview::onPatternChanged);
	layout->addWidget(m_showColorsCheck);

	layout->addStretch();
	m_patternGroupInnerLayout->addWidget(m_patternContent);
}

// =============================================================================
// Animation Player enable/disable
// =============================================================================

void AnimationPreview::updateAnimPlayerEnabled()
{
	bool hasAnimation = false;

	if (m_thingType) {
		const FrameGroup *group = m_thingType->getFrameGroup(m_currentFrameGroupType);
		if (group) {
			hasAnimation = (group->animationPhases > 1);
		}
	}

	// Disable only the playback controls when there is nothing to animate,
	// but keep the Frame Type combo usable so the user can always switch
	// between Idle / Walking regardless of the current group's frame count.
	m_playButton->setEnabled(hasAnimation);
	m_stopButton->setEnabled(hasAnimation);
	m_loopCheckBox->setEnabled(hasAnimation);
	m_phaseSlider->setEnabled(hasAnimation);
	m_phaseLabel->setEnabled(hasAnimation);
}

// =============================================================================
// Frame Type ComboBox
// =============================================================================

void AnimationPreview::updateFrameTypeCombo()
{
	// Block signals while we rebuild
	m_frameTypeCombo->blockSignals(true);
	m_frameTypeCombo->clear();

	bool showFrameType = false;

	if (m_thingType) {
		// Check if a Walking frame group exists
		bool hasWalking = (m_thingType->frameGroups[static_cast<int>(FrameGroupType::Walking)] != nullptr);

		if (hasWalking) {
			showFrameType = true;
			m_frameTypeCombo->addItem("Idle", static_cast<int>(FrameGroupType::Idle));
			m_frameTypeCombo->addItem("Walking", static_cast<int>(FrameGroupType::Walking));

			// Select the current frame group type
			int currentIdx = static_cast<int>(m_currentFrameGroupType);
			for (int i = 0; i < m_frameTypeCombo->count(); ++i) {
				if (m_frameTypeCombo->itemData(i).toInt() == currentIdx) {
					m_frameTypeCombo->setCurrentIndex(i);
					break;
				}
			}
		}
	}

	m_frameTypeLabel->setVisible(showFrameType);
	m_frameTypeCombo->setVisible(showFrameType);
	m_frameTypeCombo->blockSignals(false);
}

void AnimationPreview::onFrameTypeChanged(int index)
{
	if (index < 0)
		return;

	int fgTypeInt = m_frameTypeCombo->itemData(index).toInt();
	auto fgType = static_cast<FrameGroupType>(fgTypeInt);

	if (fgType != m_currentFrameGroupType) {
		m_currentFrameGroupType = fgType;
		stop();
		m_currentPhase = 0;
		rebuildFrameList();
		updatePatternLimits();
		updateAnimPlayerEnabled();
		displayCurrentFrame();
	}
}

// =============================================================================
// Core animation logic
// =============================================================================

void AnimationPreview::rebuildFrameList()
{
	m_frames.clear();

	if (!m_thingType || !m_spriteFile) {
		return;
	}

	const FrameGroup *group = m_thingType->getFrameGroup(m_currentFrameGroupType);
	if (!group) {
		return;
	}

	// Build a list of composite images for each animation phase.
	// A composite image is built from the sprite grid (width x height tiles)
	// for the current pattern and layer settings.
	int phases = group->animationPhases;
	int w = group->width;
	int h = group->height;

	for (int phase = 0; phase < phases; ++phase) {
		QImage compositeImage(w * SPRITE_SIZE, h * SPRITE_SIZE, QImage::Format_ARGB32);
		compositeImage.fill(Qt::transparent);
		m_frames.push_back(std::move(compositeImage));
	}

	updateFrameImages();

	m_phaseSlider->setRange(0, qMax(0, phases - 1));
	m_phaseSlider->setValue(0);
	m_phaseLabel->setText(QString("Frame: 1 / %1").arg(phases));
}

void AnimationPreview::updateFrameImages()
{
	if (!m_thingType || !m_spriteFile)
		return;

	const FrameGroup *group = m_thingType->getFrameGroup(m_currentFrameGroupType);
	if (!group)
		return;

	int phases = group->animationPhases;
	int w = group->width;
	int h = group->height;
	int layers = group->layers;
	int pxCount = group->patternX;
	int pyCount = group->patternY;
	int pzCount = group->patternZ;

	int px = qBound(0, m_patternX, pxCount - 1);
	int py = qBound(0, m_patternY, pyCount - 1);
	int pz = qBound(0, m_patternZ, pzCount - 1);
	int layer = qBound(0, m_layer, layers - 1);

	for (int phase = 0; phase < phases && phase < static_cast<int>(m_frames.size()); ++phase) {
		QImage compositeImage(w * SPRITE_SIZE, h * SPRITE_SIZE, QImage::Format_ARGB32);
		compositeImage.fill(Qt::transparent);

		QPainter painter(&compositeImage);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

		for (int ty = 0; ty < h; ++ty) {
			for (int tx = 0; tx < w; ++tx) {
				// Sprite index calculation matching the reference:
				// index = ((((((phase * pzCount + pz) * pyCount + py) * pxCount + px)
				//            * layers + layer) * h + ty) * w + tx)
				int index = ((((((phase * pzCount + pz) * pyCount + py) * pxCount + px) * layers + layer) * h + ty) * w + tx);

				if (index < 0 || index >= static_cast<int>(group->spriteIds.size())) {
					continue;
				}

				uint32_t spriteId = group->spriteIds[index];
				if (spriteId == 0)
					continue;

				QImage spriteImage = m_spriteFile->getSpriteImage(spriteId);
				if (spriteImage.isNull())
					continue;

				// Tiles are laid out right-to-left, bottom-to-top in the visual
				// representation, matching the reference client renderer
				int drawX = (w - tx - 1) * SPRITE_SIZE;
				int drawY = (h - ty - 1) * SPRITE_SIZE;
				painter.drawImage(drawX, drawY, spriteImage);
			}
		}

		painter.end();
		m_frames[phase] = std::move(compositeImage);
	}
}

void AnimationPreview::updatePatternLimits()
{
	if (!m_thingType) {
		// Reset generic controls if they exist
		if (m_patternXSpin)
			m_patternXSpin->setMaximum(0);
		if (m_patternYSpin)
			m_patternYSpin->setMaximum(0);
		if (m_patternZSpin)
			m_patternZSpin->setMaximum(0);
		if (m_layerSpin)
			m_layerSpin->setMaximum(0);

		// Reset creature controls if they exist
		if (m_directionCombo)
			m_directionCombo->setEnabled(false);
		if (m_addonCombo)
			m_addonCombo->setEnabled(false);
		if (m_stanceCombo)
			m_stanceCombo->setEnabled(false);
		if (m_stanceLabel)
			m_stanceLabel->setVisible(false);
		if (m_showColorsCheck) {
			m_showColorsCheck->setEnabled(false);
			m_showColorsCheck->setChecked(false);
		}
		return;
	}

	const FrameGroup *group = m_thingType->getFrameGroup(m_currentFrameGroupType);
	if (!group) {
		if (m_patternXSpin)
			m_patternXSpin->setMaximum(0);
		if (m_patternYSpin)
			m_patternYSpin->setMaximum(0);
		if (m_patternZSpin)
			m_patternZSpin->setMaximum(0);
		if (m_layerSpin)
			m_layerSpin->setMaximum(0);
		return;
	}

	if (m_thingType->category == ThingCategory::Creature) {
		// --- Creature-specific pattern controls ---

		// Direction ComboBox: enable items based on xPattern count
		if (m_directionCombo) {
			m_directionCombo->setEnabled(true);
			// Clamp direction selection to available xPatterns
			int maxDir = group->patternX - 1;
			if (m_directionCombo->currentData().toInt() > maxDir) {
				// Find the closest valid index
				for (int i = m_directionCombo->count() - 1; i >= 0; --i) {
					if (m_directionCombo->itemData(i).toInt() <= maxDir) {
						m_directionCombo->setCurrentIndex(i);
						break;
					}
				}
			}
		}

		// Addon ComboBox: populate dynamically based on yPattern count
		if (m_addonCombo) {
			m_addonCombo->blockSignals(true);
			int prevIndex = m_addonCombo->currentIndex();
			m_addonCombo->clear();

			int yPatCount = group->patternY;
			m_addonCombo->addItem("Base", 0);
			if (yPatCount >= 2) {
				m_addonCombo->addItem("Addon 1", 1);
			}
			if (yPatCount >= 3) {
				m_addonCombo->addItem("Addon 2", 2);
			}

			// Restore previous selection if still valid
			if (prevIndex >= 0 && prevIndex < m_addonCombo->count()) {
				m_addonCombo->setCurrentIndex(prevIndex);
			} else {
				m_addonCombo->setCurrentIndex(0);
			}
			m_addonCombo->setEnabled(yPatCount > 1);
			m_addonCombo->blockSignals(false);
		}

		// Stance ComboBox: show/enable only if zPattern > 1
		if (m_stanceCombo && m_stanceLabel) {
			bool hasMountStance = (group->patternZ > 1);
			m_stanceLabel->setVisible(hasMountStance);
			m_stanceCombo->setVisible(hasMountStance);
			m_stanceCombo->setEnabled(hasMountStance);
			if (!hasMountStance) {
				m_stanceCombo->setCurrentIndex(0);
			}
		}

		// Show Colors Checkbox: enabled when layers > 1
		if (m_showColorsCheck) {
			bool hasExtraLayers = (group->layers > 1);
			m_showColorsCheck->setEnabled(hasExtraLayers);
			if (!hasExtraLayers) {
				m_showColorsCheck->setChecked(false);
			}
		}
	} else {
		// --- Generic spinbox pattern controls ---
		if (m_patternXSpin)
			m_patternXSpin->setMaximum(qMax(0, group->patternX - 1));
		if (m_patternYSpin)
			m_patternYSpin->setMaximum(qMax(0, group->patternY - 1));
		if (m_patternZSpin)
			m_patternZSpin->setMaximum(qMax(0, group->patternZ - 1));
		if (m_layerSpin)
			m_layerSpin->setMaximum(qMax(0, group->layers - 1));
	}
}

void AnimationPreview::displayCurrentFrame()
{
	if (m_frames.empty()) {
		m_canvas->clearFrame();
		m_phaseLabel->setText("Frame: 0 / 0");
		return;
	}

	int phase = qBound(0, m_currentPhase, static_cast<int>(m_frames.size()) - 1);
	m_canvas->setFrame(m_frames[phase]);
	m_phaseLabel->setText(QString("Frame: %1 / %2").arg(phase + 1).arg(m_frames.size()));

	emit frameChanged(phase);
}

int AnimationPreview::getPhaseDuration(int phase) const
{
	if (!m_thingType)
		return 100;

	const FrameGroup *group = m_thingType->getFrameGroup(m_currentFrameGroupType);
	if (!group || !group->animator)
		return 100;

	const auto &durations = group->animator->phaseDurations;
	if (phase < 0 || phase >= static_cast<int>(durations.size()))
		return 100;

	auto [minDur, maxDur] = durations[phase];

	// Use the average of min and max for preview purposes
	if (minDur == maxDur) {
		return static_cast<int>(minDur);
	}
	return static_cast<int>((minDur + maxDur) / 2);
}

void AnimationPreview::startTimerForPhase(int phase)
{
	int duration = getPhaseDuration(phase);
	if (duration < 10)
		duration = 10;
	m_timer->start(duration);
}

void AnimationPreview::advanceFrame()
{
	if (m_frames.empty()) {
		pause();
		return;
	}

	int nextPhase = m_currentPhase + 1;
	if (nextPhase >= static_cast<int>(m_frames.size())) {
		if (m_looping) {
			nextPhase = 0;
		} else {
			pause();
			return;
		}
	}

	m_currentPhase = nextPhase;

	m_phaseSlider->blockSignals(true);
	m_phaseSlider->setValue(m_currentPhase);
	m_phaseSlider->blockSignals(false);

	displayCurrentFrame();

	if (m_playing) {
		startTimerForPhase(m_currentPhase);
	}
}

void AnimationPreview::onSliderChanged(int value)
{
	if (value == m_currentPhase)
		return;

	bool wasPlaying = m_playing;
	if (wasPlaying) {
		m_timer->stop();
	}

	m_currentPhase = value;
	displayCurrentFrame();

	if (wasPlaying) {
		startTimerForPhase(m_currentPhase);
	}
}

void AnimationPreview::onPatternChanged()
{
	if (m_thingType && m_thingType->category == ThingCategory::Creature) {
		// Read from creature-specific controls
		m_patternX = m_directionCombo ? m_directionCombo->currentData().toInt() : 0;
		m_patternY = m_addonCombo ? m_addonCombo->currentData().toInt() : 0;
		m_patternZ = m_stanceCombo ? m_stanceCombo->currentData().toInt() : 0;
		// Show Colors: unchecked = layer 0, checked = layer 1
		m_layer = (m_showColorsCheck && m_showColorsCheck->isChecked()) ? 1 : 0;
	} else {
		// Read from generic spinbox controls
		m_patternX = m_patternXSpin ? m_patternXSpin->value() : 0;
		m_patternY = m_patternYSpin ? m_patternYSpin->value() : 0;
		m_patternZ = m_patternZSpin ? m_patternZSpin->value() : 0;
		m_layer = m_layerSpin ? m_layerSpin->value() : 0;
	}

	updateFrameImages();
	displayCurrentFrame();
}
