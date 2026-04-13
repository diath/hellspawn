/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/EditorTab.h"
#include "editor/EditorContent.h"
#include "editor/LoadWidget.h"
#include "editor/SpriteEditor.h"
#include "editor/ThingEditor.h"
#include "util/Logger.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QVBoxLayout>

#include <fmt/core.h>

EditorTab::EditorTab(QWidget *parent)
: QWidget(parent)
{
	setupUi();
}

void EditorTab::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_stack = new QStackedWidget(this);
	layout->addWidget(m_stack);

	// Create the load widget (initial state)
	m_loadWidget = new LoadWidget(this);
	m_stack->addWidget(m_loadWidget);

	// Connect load widget signals
	connect(m_loadWidget, &LoadWidget::loadRequested, this, &EditorTab::loadFiles);
	connect(m_loadWidget, &LoadWidget::newRequested, this, &EditorTab::createNew);

	// Editor content is created on demand when files are loaded
	m_editorContent = nullptr;

	switchToLoadWidget();
}

void EditorTab::switchToLoadWidget()
{
	m_stack->setCurrentWidget(m_loadWidget);
}

void EditorTab::switchToEditorContent()
{
	if (!m_editorContent) {
		m_editorContent = new EditorContent(this);
		m_stack->addWidget(m_editorContent);

		connect(m_editorContent, &EditorContent::dataModified, this, &EditorTab::markDirty);

		connect(m_editorContent->spriteEditor(), &SpriteEditor::dataModified, this, [this]() { m_sprDirty = true; });
		connect(m_editorContent->thingEditor(), &ThingEditor::dataModified, this, [this]() { m_datOtbDirty = true; });
	}

	m_editorContent->setData(&m_spriteFile, &m_datFile, &m_otbFile, &m_itemsXml);
	m_stack->setCurrentWidget(m_editorContent);
}

QString EditorTab::tabTitle() const
{
	if (!m_loaded) {
		return QStringLiteral("New Tab");
	}

	// Derive a title from the dat file path, falling back to spr, then otb
	QString path = m_datPath;
	if (path.isEmpty()) {
		path = m_sprPath;
	}
	if (path.isEmpty()) {
		path = m_otbPath;
	}
	if (path.isEmpty()) {
		return QStringLiteral("Untitled");
	}

	QFileInfo info(path);
	return info.completeBaseName();
}

QString EditorTab::tabToolTip() const
{
	QStringList parts;
	if (!m_sprPath.isEmpty()) {
		parts << QStringLiteral("SPR: ") + m_sprPath;
	}
	if (!m_datPath.isEmpty()) {
		parts << QStringLiteral("DAT: ") + m_datPath;
	}
	if (!m_otbPath.isEmpty()) {
		parts << QStringLiteral("OTB: ") + m_otbPath;
	}
	if (!m_xmlPath.isEmpty()) {
		parts << QStringLiteral("XML: ") + m_xmlPath;
	}
	if (parts.isEmpty()) {
		return QStringLiteral("No files loaded");
	}
	return parts.join(QStringLiteral("\n"));
}

bool EditorTab::loadFiles()
{
	// Read paths from the load widget's combo boxes
	m_sprPath = m_loadWidget->sprPath();
	m_datPath = m_loadWidget->datPath();
	m_otbPath = m_loadWidget->otbPath();
	m_xmlPath = m_loadWidget->xmlPath();

	if (m_sprPath.isEmpty() || m_datPath.isEmpty() || m_otbPath.isEmpty()) {
		QMessageBox::warning(this, "Missing Files", "Please specify at least the .spr, .dat, and .otb file paths.");
		return false;
	}

	// Unload any previously loaded data
	m_spriteFile.unload();
	m_datFile.unload();
	m_otbFile.unload();
	m_itemsXml.unload();

	// Load SPR file
	Log().info("EditorTab: loading SPR from '{}'", m_sprPath.toStdString());
	if (!m_spriteFile.load(m_sprPath.toStdString())) {
		QMessageBox::critical(this, "Load Error", QString("Failed to load sprite file:\n%1").arg(m_sprPath));
		return false;
	}

	// Load DAT file
	Log().info("EditorTab: loading DAT from '{}'", m_datPath.toStdString());
	if (!m_datFile.load(m_datPath.toStdString())) {
		QMessageBox::critical(this, "Load Error", QString("Failed to load dat file:\n%1").arg(m_datPath));
		m_spriteFile.unload();
		return false;
	}

	// Load OTB file
	Log().info("EditorTab: loading OTB from '{}'", m_otbPath.toStdString());
	if (!m_otbFile.load(m_otbPath.toStdString())) {
		QMessageBox::critical(this, "Load Error", QString("Failed to load otb file:\n%1").arg(m_otbPath));
		m_spriteFile.unload();
		m_datFile.unload();
		return false;
	}

	// Load XML file (optional)
	if (!m_xmlPath.isEmpty()) {
		Log().info("EditorTab: loading XML from '{}'", m_xmlPath.toStdString());
		if (!m_itemsXml.load(m_xmlPath.toStdString())) {
			Log().warning("EditorTab: failed to load items.xml (non-fatal) from '{}'", m_xmlPath.toStdString());
			// Non-fatal: XML is optional, only used for display names
		}
	}

	// Remember recent paths in the load widget
	m_loadWidget->addRecentPath(LoadWidget::FieldSpr, m_sprPath);
	m_loadWidget->addRecentPath(LoadWidget::FieldDat, m_datPath);
	m_loadWidget->addRecentPath(LoadWidget::FieldOtb, m_otbPath);
	if (!m_xmlPath.isEmpty()) {
		m_loadWidget->addRecentPath(LoadWidget::FieldXml, m_xmlPath);
	}

	setLoaded(true);
	clearDirty();
	switchToEditorContent();

	Log().info("EditorTab: all files loaded successfully");
	emit titleChanged();

	return true;
}

void EditorTab::createNew()
{
	// Unload any previously loaded data
	m_spriteFile.unload();
	m_datFile.unload();
	m_otbFile.unload();
	m_itemsXml.unload();

	// Create blank data objects with default signatures/versions
	m_spriteFile.createEmpty(0x00000000);
	m_datFile.createEmpty(0x00000000);
	m_otbFile.createEmpty(3, 56, 0);

	// Clear file paths since this is a new document
	m_sprPath.clear();
	m_datPath.clear();
	m_otbPath.clear();
	m_xmlPath.clear();

	setLoaded(true);
	m_sprDirty = true;
	m_datOtbDirty = true;
	markDirty(); // New documents are immediately dirty (need Save As)
	switchToEditorContent();

	Log().info("EditorTab: created new blank editor state");
	emit titleChanged();
}

bool EditorTab::save()
{
	if (!m_loaded) {
		return false;
	}

	// If we don't have file paths yet, fall through to Save As
	if (m_sprPath.isEmpty() || m_datPath.isEmpty() || m_otbPath.isEmpty()) {
		return saveAs();
	}

	bool success = true;

	// Save SPR (only if sprite data was modified)
	if (m_sprDirty) {
		Log().info("EditorTab: saving SPR to '{}'", m_sprPath.toStdString());
		if (!m_spriteFile.save(m_sprPath.toStdString())) {
			QMessageBox::critical(this, "Save Error", QString("Failed to save sprite file:\n%1").arg(m_sprPath));
			success = false;
		}
	} else {
		Log().info("EditorTab: skipping SPR save (no modifications)");
	}

	// Save DAT (only if thing data was modified)
	if (success && m_datOtbDirty) {
		Log().info("EditorTab: saving DAT to '{}'", m_datPath.toStdString());
		if (!m_datFile.save(m_datPath.toStdString())) {
			QMessageBox::critical(this, "Save Error", QString("Failed to save dat file:\n%1").arg(m_datPath));
			success = false;
		}
	} else if (success) {
		Log().info("EditorTab: skipping DAT save (no modifications)");
	}

	// Save OTB (only if thing data was modified)
	if (success && m_datOtbDirty) {
		Log().info("EditorTab: saving OTB to '{}'", m_otbPath.toStdString());
		if (!m_otbFile.save(m_otbPath.toStdString())) {
			QMessageBox::critical(this, "Save Error", QString("Failed to save otb file:\n%1").arg(m_otbPath));
			success = false;
		}
	} else if (success) {
		Log().info("EditorTab: skipping OTB save (no modifications)");
	}

	if (success) {
		clearDirty();
		Log().info("EditorTab: all files saved successfully");
	}

	return success;
}

bool EditorTab::saveAs()
{
	if (!m_loaded) {
		return false;
	}

	// Prompt for SPR path
	QString sprPath = QFileDialog::getSaveFileName(this, "Save SPR File", m_sprPath.isEmpty() ? QDir::currentPath() : m_sprPath, "Sprite Files (*.spr);;All Files (*)");
	if (sprPath.isEmpty()) {
		return false;
	}

	// Prompt for DAT path
	QString datPath = QFileDialog::getSaveFileName(this, "Save DAT File", m_datPath.isEmpty() ? QFileInfo(sprPath).absolutePath() : m_datPath, "Data Files (*.dat);;All Files (*)");
	if (datPath.isEmpty()) {
		return false;
	}

	// Prompt for OTB path
	QString otbPath = QFileDialog::getSaveFileName(this, "Save OTB File", m_otbPath.isEmpty() ? QFileInfo(sprPath).absolutePath() : m_otbPath, "OTB Files (*.otb);;All Files (*)");
	if (otbPath.isEmpty()) {
		return false;
	}

	// Update paths
	m_sprPath = sprPath;
	m_datPath = datPath;
	m_otbPath = otbPath;

	// Remember new paths in load widget
	m_loadWidget->addRecentPath(LoadWidget::FieldSpr, m_sprPath);
	m_loadWidget->addRecentPath(LoadWidget::FieldDat, m_datPath);
	m_loadWidget->addRecentPath(LoadWidget::FieldOtb, m_otbPath);

	emit titleChanged();

	// Save As always writes all files to the new locations
	m_sprDirty = true;
	m_datOtbDirty = true;

	return save();
}

bool EditorTab::reload()
{
	if (!m_loaded) {
		return false;
	}

	if (m_sprPath.isEmpty() || m_datPath.isEmpty() || m_otbPath.isEmpty()) {
		QMessageBox::warning(this, "Reload Error", "Cannot reload — no file paths are set. This is a new document.");
		return false;
	}

	if (m_dirty) {
		QMessageBox::StandardButton result = QMessageBox::question(
		    this, "Unsaved Changes",
		    "There are unsaved changes. Reloading will discard all changes.\n\nContinue?",
		    QMessageBox::Yes | QMessageBox::No, QMessageBox::No
		);

		if (result != QMessageBox::Yes) {
			return false;
		}
	}

	// Unload existing data
	m_spriteFile.unload();
	m_datFile.unload();
	m_otbFile.unload();
	m_itemsXml.unload();

	// Reload from disk
	bool success = true;

	if (!m_spriteFile.load(m_sprPath.toStdString())) {
		QMessageBox::critical(this, "Reload Error", QString("Failed to reload sprite file:\n%1").arg(m_sprPath));
		success = false;
	}

	if (success && !m_datFile.load(m_datPath.toStdString())) {
		QMessageBox::critical(this, "Reload Error", QString("Failed to reload dat file:\n%1").arg(m_datPath));
		success = false;
	}

	if (success && !m_otbFile.load(m_otbPath.toStdString())) {
		QMessageBox::critical(this, "Reload Error", QString("Failed to reload otb file:\n%1").arg(m_otbPath));
		success = false;
	}

	if (success && !m_xmlPath.isEmpty()) {
		m_itemsXml.load(m_xmlPath.toStdString());
	}

	if (success) {
		clearDirty();
		// Refresh the editor content
		if (m_editorContent) {
			m_editorContent->setData(&m_spriteFile, &m_datFile, &m_otbFile, &m_itemsXml);
		}
		Log().info("EditorTab: reloaded all files successfully");
	} else {
		// If reload failed, go back to load widget
		setLoaded(false);
		switchToLoadWidget();
		Log().error("EditorTab: reload failed");
	}

	return success;
}

void EditorTab::markDirty()
{
	setDirty(true);
}

void EditorTab::clearDirty()
{
	m_sprDirty = false;
	m_datOtbDirty = false;
	setDirty(false);
}

void EditorTab::setDirty(bool dirty)
{
	if (m_dirty != dirty) {
		m_dirty = dirty;
		emit dirtyChanged(m_dirty);
	}
}

void EditorTab::setLoaded(bool loaded)
{
	if (m_loaded != loaded) {
		m_loaded = loaded;
		emit loadStateChanged(m_loaded);
	}
}
