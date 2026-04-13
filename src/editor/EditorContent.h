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

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class SpriteEditor;
class ThingEditor;

class EditorContent: public QWidget
{
		Q_OBJECT

	public:
		explicit EditorContent(QWidget *parent = nullptr);
		~EditorContent() override = default;

		void setData(SpriteFile *spriteFile, DatFile *datFile, OtbFile *otbFile, ItemsXml *itemsXml);

		SpriteFile *spriteFile() const { return m_spriteFile; }
		DatFile *datFile() const { return m_datFile; }
		OtbFile *otbFile() const { return m_otbFile; }
		ItemsXml *itemsXml() const { return m_itemsXml; }

		SpriteEditor *spriteEditor() const { return m_spriteEditor; }
		ThingEditor *thingEditor() const { return m_thingEditor; }

	signals:
		void dataModified();

	private:
		void setupUi();

		QTabWidget *m_subTabs = nullptr;
		SpriteEditor *m_spriteEditor = nullptr;
		ThingEditor *m_thingEditor = nullptr;

		SpriteFile *m_spriteFile = nullptr;
		DatFile *m_datFile = nullptr;
		OtbFile *m_otbFile = nullptr;
		ItemsXml *m_itemsXml = nullptr;
};
