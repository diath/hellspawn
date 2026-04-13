/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/SpriteEditor.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

SpriteEditor::SpriteEditor(QWidget *parent)
: QWidget(parent)
{
	setupUi();
}

void SpriteEditor::setSpriteFile(SpriteFile *spriteFile)
{
	m_spriteFile = spriteFile;
	m_spriteGrid->setSpriteFile(spriteFile);
	updatePreview();
	updateSignatureFields();
}

SpriteFile *SpriteEditor::spriteFile() const { return m_spriteFile; }

void SpriteEditor::setupUi()
{
	auto *mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(6);

	// Left side: sprite grid
	auto *leftLayout = new QVBoxLayout;
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(4);

	// Grid size controls
	auto *gridControlLayout = new QHBoxLayout;
	gridControlLayout->setSpacing(4);

	gridControlLayout->addWidget(new QLabel("Columns:"));
	m_columnsSpin = new QSpinBox;
	m_columnsSpin->setRange(1, 50);
	m_columnsSpin->setValue(24);
	m_columnsSpin->setFixedWidth(60);
	connect(m_columnsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &SpriteEditor::onGridSizeChanged);
	gridControlLayout->addWidget(m_columnsSpin);

	gridControlLayout->addWidget(new QLabel("Rows:"));
	m_rowsSpin = new QSpinBox;
	m_rowsSpin->setRange(1, 50);
	m_rowsSpin->setValue(12);
	m_rowsSpin->setFixedWidth(60);
	connect(m_rowsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &SpriteEditor::onGridSizeChanged);
	gridControlLayout->addWidget(m_rowsSpin);

	gridControlLayout->addSpacing(12);

	gridControlLayout->addWidget(new QLabel("Go to ID:"));
	m_goToSpin = new QSpinBox;
	m_goToSpin->setRange(1, 999999999);
	m_goToSpin->setValue(1);
	m_goToSpin->setFixedWidth(100);
	m_goToSpin->setKeyboardTracking(false);
	connect(m_goToSpin, &QSpinBox::editingFinished, this, [this]() {
		if (m_goToSpin->hasFocus())
			onGoToClicked();
	});
	gridControlLayout->addWidget(m_goToSpin);

	auto *goToButton = new QPushButton("Go");
	goToButton->setFixedWidth(40);
	connect(goToButton, &QPushButton::clicked, this, &SpriteEditor::onGoToClicked);
	gridControlLayout->addWidget(goToButton);

	gridControlLayout->addStretch();

	leftLayout->addLayout(gridControlLayout);

	// Sprite grid
	m_spriteGrid = new SpriteGrid;
	connect(m_spriteGrid, &SpriteGrid::selectionChanged, this, &SpriteEditor::onSelectionChanged);
	connect(m_spriteGrid, &SpriteGrid::spriteDoubleClicked, this, &SpriteEditor::onSpriteDoubleClicked);
	connect(m_spriteGrid, &SpriteGrid::spriteContextMenu, this, &SpriteEditor::onSpriteContextMenu);
	leftLayout->addWidget(m_spriteGrid, 1);

	mainLayout->addLayout(leftLayout, 1);

	// Right side: preview and actions panel
	auto *rightPanel = new QVBoxLayout;
	rightPanel->setContentsMargins(0, 0, 0, 0);
	rightPanel->setSpacing(8);

	// Signature group
	auto *signatureGroup = new QGroupBox("Signature");
	auto *signatureFormLayout = new QFormLayout(signatureGroup);
	signatureFormLayout->setContentsMargins(8, 12, 8, 8);
	signatureFormLayout->setSpacing(4);

	m_signatureEdit = new QLineEdit;
	m_signatureEdit->setPlaceholderText("Enter numeric signature");
	auto *signatureValidator = new QRegularExpressionValidator(
	    QRegularExpression(QStringLiteral("[0-9]{0,10}")), m_signatureEdit
	);
	m_signatureEdit->setValidator(signatureValidator);
	connect(m_signatureEdit, &QLineEdit::textChanged, this, &SpriteEditor::onSignatureTextChanged);
	signatureFormLayout->addRow("Decimal:", m_signatureEdit);

	m_signatureHexEdit = new QLineEdit;
	m_signatureHexEdit->setReadOnly(true);
	m_signatureHexEdit->setPlaceholderText("0x00000000");
	signatureFormLayout->addRow("Hex:", m_signatureHexEdit);

	rightPanel->addWidget(signatureGroup);

	// Preview group
	auto *previewGroup = new QGroupBox("Preview");
	auto *previewLayout = new QVBoxLayout(previewGroup);
	previewLayout->setContentsMargins(8, 12, 8, 8);
	previewLayout->setSpacing(4);

	m_previewWidget = new SpritePreview;
	m_previewWidget->setScale(4);
	m_previewWidget->setMinimumSize(134, 134);
	m_previewWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	previewLayout->addWidget(m_previewWidget, 0, Qt::AlignCenter);

	m_previewIdLabel = new QLabel("No selection");
	m_previewIdLabel->setAlignment(Qt::AlignCenter);
	previewLayout->addWidget(m_previewIdLabel);

	rightPanel->addWidget(previewGroup);

	// Actions group
	auto *actionsGroup = new QGroupBox("Actions");
	auto *actionsLayout = new QVBoxLayout(actionsGroup);
	actionsLayout->setContentsMargins(8, 12, 8, 8);
	actionsLayout->setSpacing(4);

	m_exportButton = new QPushButton("Export Selected");
	m_exportButton->setToolTip("Export selected sprites as PNG images");
	m_exportButton->setEnabled(false);
	connect(m_exportButton, &QPushButton::clicked, this, &SpriteEditor::onExportClicked);
	actionsLayout->addWidget(m_exportButton);

	m_importNewButton = new QPushButton("Import (New)");
	m_importNewButton->setToolTip("Import an image as a new sprite");
	connect(m_importNewButton, &QPushButton::clicked, this, &SpriteEditor::onImportNewClicked);
	actionsLayout->addWidget(m_importNewButton);

	m_importOverrideButton = new QPushButton("Import (Override)");
	m_importOverrideButton->setToolTip("Replace the selected sprite with an imported image");
	m_importOverrideButton->setEnabled(false);
	connect(m_importOverrideButton, &QPushButton::clicked, this, &SpriteEditor::onImportOverrideClicked);
	actionsLayout->addWidget(m_importOverrideButton);

	m_clearButton = new QPushButton("Clear Selected");
	m_clearButton->setToolTip("Clear the selected sprite(s) to transparent");
	m_clearButton->setEnabled(false);
	connect(m_clearButton, &QPushButton::clicked, this, &SpriteEditor::onClearClicked);
	actionsLayout->addWidget(m_clearButton);

	actionsLayout->addSpacing(8);

	m_copyButton = new QPushButton("Copy to Clipboard");
	m_copyButton->setToolTip("Copy the selected sprite image to clipboard");
	m_copyButton->setEnabled(false);
	connect(m_copyButton, &QPushButton::clicked, this, &SpriteEditor::onCopyClicked);
	actionsLayout->addWidget(m_copyButton);

	m_pasteButton = new QPushButton("Paste from Clipboard");
	m_pasteButton->setToolTip("Paste an image from clipboard into the selected sprite");
	m_pasteButton->setEnabled(false);
	connect(m_pasteButton, &QPushButton::clicked, this, &SpriteEditor::onPasteClicked);
	actionsLayout->addWidget(m_pasteButton);

	rightPanel->addWidget(actionsGroup);

	rightPanel->addStretch();

	mainLayout->addLayout(rightPanel, 0);
}

void SpriteEditor::updatePreview()
{
	if (!m_spriteFile) {
		m_previewWidget->clearImage();
		m_previewIdLabel->setText("No file loaded");
		return;
	}

	uint32_t selectedId = m_spriteGrid->singleSelectedId();
	if (selectedId == 0) {
		QSet<uint32_t> ids = m_spriteGrid->selectedSpriteIds();
		if (ids.isEmpty()) {
			m_previewWidget->clearImage();
			m_previewIdLabel->setText("No selection");
		} else {
			m_previewWidget->clearImage();
			m_previewIdLabel->setText(QString("%1 sprites selected").arg(ids.size()));
		}
		return;
	}

	QImage image = m_spriteFile->getSpriteImage(selectedId);
	if (image.isNull()) {
		m_previewWidget->clearImage();
		m_previewIdLabel->setText(QString("Sprite #%1 (empty)").arg(selectedId));
	} else {
		m_previewWidget->setSpriteImage(image, selectedId);
		m_previewIdLabel->setText(QString("Sprite #%1").arg(selectedId));
	}
}

void SpriteEditor::updateSignatureFields()
{
	if (!m_spriteFile || !m_spriteFile->isLoaded()) {
		m_signatureEdit->clear();
		m_signatureHexEdit->clear();
		return;
	}

	// Block signals to avoid recursive updates while populating
	QSignalBlocker blocker(m_signatureEdit);
	m_signatureEdit->setText(QString::number(m_spriteFile->signature()));
	m_signatureHexEdit->setText(
	    QString("0x%1").arg(m_spriteFile->signature(), 8, 16, QChar('0'))
	);
}

void SpriteEditor::onSignatureTextChanged(const QString &text)
{
	bool ok = false;
	uint64_t value = text.toULongLong(&ok);
	if (!ok || value > std::numeric_limits<uint32_t>::max()) {
		m_signatureHexEdit->setText(QString());
		return;
	}

	auto sig = static_cast<uint32_t>(value);
	m_signatureHexEdit->setText(
	    QString("0x%1").arg(sig, 8, 16, QChar('0'))
	);

	if (m_spriteFile && m_spriteFile->isLoaded()) {
		m_spriteFile->setSignature(sig);
		emit dataModified();
	}
}

void SpriteEditor::updateActionButtons()
{
	QSet<uint32_t> sel = m_spriteGrid->selectedSpriteIds();
	bool hasSelection = !sel.isEmpty();
	bool hasSingleSelection = (sel.size() == 1);
	bool hasFile = (m_spriteFile != nullptr && m_spriteFile->isLoaded());

	m_exportButton->setEnabled(hasSelection && hasFile);
	m_importOverrideButton->setEnabled(hasSingleSelection && hasFile);
	m_clearButton->setEnabled(hasSelection && hasFile);
	m_copyButton->setEnabled(hasSingleSelection && hasFile);
	m_pasteButton->setEnabled(hasSingleSelection && hasFile);
	m_importNewButton->setEnabled(hasFile);
}

void SpriteEditor::onSelectionChanged(const QSet<uint32_t> & /*selectedIds*/)
{
	updatePreview();
	updateActionButtons();
}

void SpriteEditor::onSpriteDoubleClicked(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	// Double-click exports the sprite
	QString defaultName = QString("sprite_%1.png").arg(spriteId);
	QString path = QFileDialog::getSaveFileName(this, "Export Sprite", defaultName, "PNG Images (*.png);;BMP Images (*.bmp);;All Files (*)");
	if (path.isEmpty())
		return;

	if (m_spriteFile->exportSprite(spriteId, path.toStdString())) {
		QMessageBox::information(this, "Export", QString("Sprite #%1 exported successfully.").arg(spriteId));
	} else {
		QMessageBox::warning(this, "Export Failed", QString("Failed to export sprite #%1.").arg(spriteId));
	}
}

void SpriteEditor::onSpriteContextMenu(uint32_t spriteId, const QPoint &globalPos)
{
	if (!m_spriteFile)
		return;

	QMenu menu(this);

	QAction *exportAction = menu.addAction("Export...");
	connect(exportAction, &QAction::triggered, this, [this, spriteId]() {
		if (spriteId == 0)
			return;
		onSpriteDoubleClicked(spriteId);
	});

	QAction *importOverrideAction = menu.addAction("Import (Override)...");
	connect(importOverrideAction, &QAction::triggered, this, [this, spriteId]() {
		importOverrideSingle(spriteId);
	});

	QAction *clearAction = menu.addAction("Clear");
	connect(clearAction, &QAction::triggered, this, [this, spriteId]() {
		clearSingle(spriteId);
	});

	menu.addSeparator();

	QAction *copyAction = menu.addAction("Copy to Clipboard");
	connect(copyAction, &QAction::triggered, this, [this, spriteId]() {
		copySpriteToClipboard(spriteId);
	});

	QAction *pasteAction = menu.addAction("Paste from Clipboard");
	connect(pasteAction, &QAction::triggered, this, [this, spriteId]() {
		pasteSpriteFromClipboard(spriteId);
	});

	menu.addSeparator();

	QAction *goToAction = menu.addAction(QString("Sprite #%1").arg(spriteId));
	goToAction->setEnabled(false);

	menu.exec(globalPos);
}

void SpriteEditor::onGridSizeChanged()
{
	int cols = m_columnsSpin->value();
	int rows = m_rowsSpin->value();
	m_spriteGrid->setGridSize(cols, rows);
}

void SpriteEditor::onGoToClicked()
{
	uint32_t id = static_cast<uint32_t>(m_goToSpin->value());
	m_spriteGrid->goToSpriteId(id);
}

void SpriteEditor::onExportClicked()
{
	if (!m_spriteFile)
		return;

	QSet<uint32_t> selectedIds = m_spriteGrid->selectedSpriteIds();
	if (selectedIds.isEmpty())
		return;

	if (selectedIds.size() == 1) {
		uint32_t id = *selectedIds.begin();
		onSpriteDoubleClicked(id);
		return;
	}

	// Multiple sprites: export to a directory
	QString dir = QFileDialog::getExistingDirectory(this, "Export Sprites to Directory");
	if (dir.isEmpty())
		return;

	int successCount = 0;
	int failCount = 0;

	for (uint32_t id: selectedIds) {
		QString filePath = QString("%1/sprite_%2.png").arg(dir).arg(id);
		if (m_spriteFile->exportSprite(id, filePath.toStdString())) {
			++successCount;
		} else {
			++failCount;
		}
	}

	QMessageBox::information(this, "Export Complete", QString("Exported %1 sprite(s) successfully.\n%2 failed.").arg(successCount).arg(failCount));
}

void SpriteEditor::onImportNewClicked()
{
	if (!m_spriteFile)
		return;

	QStringList paths = QFileDialog::getOpenFileNames(this, "Import Sprite Images", QString(), "Images (*.png *.bmp *.jpg *.jpeg);;All Files (*)");

	if (paths.isEmpty())
		return;

	uint32_t firstNewId = 0;
	int importedCount = 0;

	for (const QString &path: paths) {
		QImage image(path);
		if (image.isNull()) {
			QMessageBox::warning(this, "Import Failed", QString("Failed to load image:\n%1").arg(path));
			continue;
		}

		uint32_t newId = m_spriteFile->addSprite(image);
		if (newId > 0) {
			if (firstNewId == 0)
				firstNewId = newId;
			++importedCount;
		}
	}

	if (importedCount > 0) {
		emit dataModified();
		m_spriteGrid->setSpriteFile(m_spriteFile);
		if (firstNewId > 0) {
			m_spriteGrid->goToSpriteId(firstNewId);
		}
		updateSignatureFields();

		QMessageBox::information(this, "Import Complete", QString("Imported %1 new sprite(s).\nFirst new ID: %2").arg(importedCount).arg(firstNewId));
	}
}

void SpriteEditor::onImportOverrideClicked()
{
	uint32_t id = m_spriteGrid->singleSelectedId();
	if (id == 0)
		return;
	importOverrideSingle(id);
}

void SpriteEditor::importOverrideSingle(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	QString path = QFileDialog::getOpenFileName(this, "Import Sprite Image", QString(), "Images (*.png *.bmp *.jpg *.jpeg);;All Files (*)");
	if (path.isEmpty())
		return;

	QImage image(path);
	if (image.isNull()) {
		QMessageBox::warning(this, "Import Failed", QString("Failed to load image:\n%1").arg(path));
		return;
	}

	if (m_spriteFile->setSpriteImage(spriteId, image)) {
		emit dataModified();
		m_spriteGrid->refresh();
		updatePreview();
	} else {
		QMessageBox::warning(this, "Import Failed", QString("Failed to set sprite #%1.").arg(spriteId));
	}
}

void SpriteEditor::onClearClicked()
{
	if (!m_spriteFile)
		return;

	QSet<uint32_t> selectedIds = m_spriteGrid->selectedSpriteIds();
	if (selectedIds.isEmpty())
		return;

	QMessageBox::StandardButton result = QMessageBox::question(this, "Clear Sprites", QString("Are you sure you want to clear %1 sprite(s)?").arg(selectedIds.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (result != QMessageBox::Yes)
		return;

	for (uint32_t id: selectedIds) {
		clearSingle(id);
	}
}

void SpriteEditor::clearSingle(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	if (m_spriteFile->clearSprite(spriteId)) {
		emit dataModified();
		m_spriteGrid->refresh();
		updatePreview();
	}
}

void SpriteEditor::onCopyClicked()
{
	uint32_t id = m_spriteGrid->singleSelectedId();
	copySpriteToClipboard(id);
}

void SpriteEditor::copySpriteToClipboard(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	QImage image = m_spriteFile->getSpriteImage(spriteId);
	if (image.isNull()) {
		QMessageBox::information(this, "Copy", "The selected sprite is empty.");
		return;
	}

	QApplication::clipboard()->setImage(image);
}

void SpriteEditor::onPasteClicked()
{
	uint32_t id = m_spriteGrid->singleSelectedId();
	pasteSpriteFromClipboard(id);
}

void SpriteEditor::pasteSpriteFromClipboard(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if (!mimeData || !mimeData->hasImage()) {
		QMessageBox::information(this, "Paste", "No image found in clipboard.");
		return;
	}

	QImage image = qvariant_cast<QImage>(mimeData->imageData());
	if (image.isNull()) {
		QMessageBox::information(this, "Paste", "Failed to read image from clipboard.");
		return;
	}

	if (m_spriteFile->setSpriteImage(spriteId, image)) {
		emit dataModified();
		m_spriteGrid->refresh();
		updatePreview();
	}
}
