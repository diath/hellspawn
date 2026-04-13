/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/DatFile.h"
#include "core/ItemsXml.h"
#include "core/OtbFile.h"
#include "core/SpriteFile.h"

#include <QStackedWidget>
#include <QString>
#include <QWidget>

#include <memory>

class LoadWidget;
class EditorContent;

class EditorTab: public QWidget
{
		Q_OBJECT

	public:
		explicit EditorTab(QWidget *parent = nullptr);
		~EditorTab() override = default;

		EditorTab(const EditorTab &) = delete;
		EditorTab &operator=(const EditorTab &) = delete;

		// State queries
		bool isLoaded() const { return m_loaded; }
		bool isDirty() const { return m_dirty; }

		QString tabTitle() const;
		QString tabToolTip() const;

		// Data accessors — each tab owns its own data instances
		SpriteFile &spriteFile() { return m_spriteFile; }
		const SpriteFile &spriteFile() const { return m_spriteFile; }

		DatFile &datFile() { return m_datFile; }
		const DatFile &datFile() const { return m_datFile; }

		OtbFile &otbFile() { return m_otbFile; }
		const OtbFile &otbFile() const { return m_otbFile; }

		ItemsXml &itemsXml() { return m_itemsXml; }
		const ItemsXml &itemsXml() const { return m_itemsXml; }

		// File paths configured via the load widget
		QString sprFilePath() const { return m_sprPath; }
		QString datFilePath() const { return m_datPath; }
		QString otbFilePath() const { return m_otbPath; }
		QString xmlFilePath() const { return m_xmlPath; }

		void setSprFilePath(const QString &path) { m_sprPath = path; }
		void setDatFilePath(const QString &path) { m_datPath = path; }
		void setOtbFilePath(const QString &path) { m_otbPath = path; }
		void setXmlFilePath(const QString &path) { m_xmlPath = path; }

	public slots:
		// Load files from the configured paths
		bool loadFiles();

		// Create a new blank editor state (no files)
		void createNew();

		// Save all loaded data to original paths
		bool save();

		// Save all loaded data to new paths (prompts for each)
		bool saveAs();

		// Reload files from disk, discarding changes
		bool reload();

		// Mark editor as dirty (unsaved changes)
		void markDirty();

		// Clear dirty flag (after successful save)
		void clearDirty();

	signals:
		void dirtyChanged(bool dirty);
		void titleChanged();
		void loadStateChanged(bool loaded);

	private:
		void setupUi();
		void switchToLoadWidget();
		void switchToEditorContent();
		void setDirty(bool dirty);
		void setLoaded(bool loaded);

		// Stacked widget to switch between load screen and editor content
		QStackedWidget *m_stack = nullptr;
		LoadWidget *m_loadWidget = nullptr;
		EditorContent *m_editorContent = nullptr;

		// Data objects — each tab owns its own independent instances
		SpriteFile m_spriteFile;
		DatFile m_datFile;
		OtbFile m_otbFile;
		ItemsXml m_itemsXml;

		// File paths
		QString m_sprPath;
		QString m_datPath;
		QString m_otbPath;
		QString m_xmlPath;

		// State
		bool m_loaded = false;
		bool m_dirty = false;

		// Per-file dirty flags — track which files actually need saving
		bool m_sprDirty = false;
		bool m_datOtbDirty = false;
};
