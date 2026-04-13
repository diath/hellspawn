/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/SpriteFile.h"
#include "widgets/SpriteGrid.h"
#include "widgets/SpritePreview.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QWidget>

#include <cstdint>

class SpriteEditor: public QWidget
{
		Q_OBJECT

	public:
		explicit SpriteEditor(QWidget *parent = nullptr);
		~SpriteEditor() override = default;

		void setSpriteFile(SpriteFile *spriteFile);
		SpriteFile *spriteFile() const;

	signals:
		void dataModified();

	private:
		void setupUi();

		void updatePreview();
		void updateSignatureFields();
		void updateActionButtons();

		void onSelectionChanged(const QSet<uint32_t> &selectedIds);
		void onSpriteDoubleClicked(uint32_t spriteId);
		void onSpriteContextMenu(uint32_t spriteId, const QPoint &globalPos);
		void onGridSizeChanged();
		void onGoToClicked();
		void onSignatureTextChanged(const QString &text);

		void onExportClicked();
		void onImportNewClicked();
		void onImportOverrideClicked();
		void onClearClicked();
		void onCopyClicked();
		void onPasteClicked();

		void importOverrideSingle(uint32_t spriteId);
		void clearSingle(uint32_t spriteId);
		void copySpriteToClipboard(uint32_t spriteId);
		void pasteSpriteFromClipboard(uint32_t spriteId);

		SpriteFile *m_spriteFile = nullptr;

		// Left panel
		SpriteGrid *m_spriteGrid = nullptr;
		QSpinBox *m_columnsSpin = nullptr;
		QSpinBox *m_rowsSpin = nullptr;
		QSpinBox *m_goToSpin = nullptr;

		// Right panel
		SpritePreview *m_previewWidget = nullptr;
		QLabel *m_previewIdLabel = nullptr;
		// Signature fields
		QLineEdit *m_signatureEdit = nullptr;
		QLineEdit *m_signatureHexEdit = nullptr;

		// Action buttons
		QPushButton *m_exportButton = nullptr;
		QPushButton *m_importNewButton = nullptr;
		QPushButton *m_importOverrideButton = nullptr;
		QPushButton *m_clearButton = nullptr;
		QPushButton *m_copyButton = nullptr;
		QPushButton *m_pasteButton = nullptr;
};
