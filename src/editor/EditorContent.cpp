/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/EditorContent.h"
#include "editor/SpriteEditor.h"
#include "editor/ThingEditor.h"

EditorContent::EditorContent(QWidget *parent)
: QWidget(parent)
{
	setupUi();
}

void EditorContent::setupUi()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_subTabs = new QTabWidget(this);
	m_subTabs->setDocumentMode(false);
	m_subTabs->setTabPosition(QTabWidget::North);

	m_spriteEditor = new SpriteEditor(this);
	m_subTabs->addTab(m_spriteEditor, "Sprites");

	m_thingEditor = new ThingEditor(this);
	m_subTabs->addTab(m_thingEditor, "Things");

	layout->addWidget(m_subTabs);

	connect(m_spriteEditor, &SpriteEditor::dataModified, this, &EditorContent::dataModified);
	connect(m_thingEditor, &ThingEditor::dataModified, this, &EditorContent::dataModified);
}

void EditorContent::setData(SpriteFile *spriteFile, DatFile *datFile, OtbFile *otbFile, ItemsXml *itemsXml)
{
	m_spriteFile = spriteFile;
	m_datFile = datFile;
	m_otbFile = otbFile;
	m_itemsXml = itemsXml;

	m_spriteEditor->setSpriteFile(spriteFile);
	m_thingEditor->setData(spriteFile, datFile, otbFile, itemsXml);
}
