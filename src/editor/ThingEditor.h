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
#include "core/ThingData.h"
#include "editor/ThingIconProvider.h"
#include "editor/ThingPropertyEditor.h"
#include "widgets/SpritePreview.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

class ThingEditor: public QWidget
{
		Q_OBJECT

	public:
		static constexpr int ICON_SIZE = 64;

		explicit ThingEditor(QWidget *parent = nullptr);
		~ThingEditor() override = default;

		void setData(SpriteFile *spriteFile, DatFile *datFile, OtbFile *otbFile, ItemsXml *itemsXml);

		SpriteFile *spriteFile() const;
		DatFile *datFile() const;
		OtbFile *otbFile() const;
		ItemsXml *itemsXml() const;

		// Clipboard support — static so it persists across all ThingEditor instances
		static bool hasClipboardThing();
		static const ThingType *clipboardThing();
		static void clearClipboardThing();

	signals:
		void dataModified();

	private:
		void setupUi();

		// --- List management ---
		QListWidget *activeList() const;
		void switchToList(QListWidget *list);

		QListWidget *createThingListWidget();
		void connectListSignals(QListWidget *list);

		void populateAllCategoryLists();
		void populateCategoryList(ThingCategory category);
		void populateSearchList(const QString &searchText);
		void rebuildCurrentCategoryList();

		void selectItemById(QListWidget *list, uint32_t thingId);

		// --- Targeted list operations ---
		QListWidgetItem *findListItem(QListWidget *list, uint32_t thingId) const;
		void appendThingItem(QListWidget *list, uint16_t thingId, ThingCategory category, const ThingType *thing);
		void removeThingItem(QListWidget *list, uint32_t thingId);
		void updateCountLabel();

		// --- Display helpers ---
		QString buildDisplayText(uint16_t thingId, ThingCategory category, const ThingType *thing) const;
		QString getThingName(uint16_t clientId, ThingCategory category) const;
		QIcon buildThingIcon(uint16_t thingId, ThingCategory category, const ThingType *thing) const;
		QIcon buildThingIconSync(uint16_t thingId, ThingCategory category, const ThingType *thing) const;
		ThingCategory currentCategory() const;

		// --- Display text cache ---
		void clearDisplayTextCache();
		void invalidateDisplayText(ThingCategory category, uint16_t thingId);

		// --- Icon cache (delegated to ThingIconProvider) ---
		void clearIconCache();
		void invalidateIcon(ThingCategory category, uint16_t thingId);

		// --- Slots ---
		void onCategoryChanged(int index);
		void onSearchTextChanged(const QString &text);
		void onSearchDebounceTimeout();
		void onSignatureTextChanged(const QString &text);
		void onThingSelected(QListWidgetItem *current, QListWidgetItem *previous);
		void onThingPropertyModified();
		void onThingSpriteModified();
		void onAddNewClicked();
		void onRemoveClicked();
		void onThingContextMenu(const QPoint &pos);
		void copyThing(uint16_t thingId, ThingCategory category);
		void duplicateThing(uint16_t thingId, ThingCategory category);

		// --- Update helpers after data mutation ---
		void updateSignatureFields();
		void updateListItemDisplay(QListWidgetItem *item, uint16_t thingId, ThingCategory category, const ThingType *thing);

		// --- Async icon delivery ---
		void onAsyncIconReady(ThingCategory category, uint16_t thingId, QIcon icon);

		// --- Reverse lookup helpers ---
		void registerItemInMap(QListWidget *list, uint16_t thingId, QListWidgetItem *item);
		void unregisterItemFromMap(QListWidget *list, uint16_t thingId);
		void clearItemMap(QListWidget *list);
		QListWidgetItem *findItemInMap(QListWidget *list, uint16_t thingId) const;

		// Static clipboard ThingType shared across all ThingEditor instances
		static std::shared_ptr<ThingType> s_clipboardThing;

		SpriteFile *m_spriteFile = nullptr;
		DatFile *m_datFile = nullptr;
		OtbFile *m_otbFile = nullptr;
		ItemsXml *m_itemsXml = nullptr;

		ThingCategory m_currentCategory = ThingCategory::Item;
		bool m_populatingList = false;
		bool m_searchActive = false;

		// Async icon provider — background worker + LRU cache
		ThingIconProvider *m_iconProvider = nullptr;

		// Per-category cache of display text strings built by buildDisplayText().
		// Keyed by thingId. Cleared per-category in populateCategoryList() and
		// selectively invalidated in onThingPropertyModified().
		mutable std::array<std::unordered_map<uint16_t, QString>, THING_CATEGORY_COUNT> m_displayTextCache;

		// Reverse lookup maps: thingId → QListWidgetItem* for O(1) icon delivery.
		// One map per category list, plus one for the search list.
		std::array<std::unordered_map<uint16_t, QListWidgetItem *>, THING_CATEGORY_COUNT> m_categoryItemMap;
		std::unordered_map<uint16_t, QListWidgetItem *> m_searchItemMap;

		// UI elements
		QSplitter *m_splitter = nullptr;
		QComboBox *m_categoryCombo = nullptr;
		QLineEdit *m_searchEdit = nullptr;
		QLabel *m_countLabel = nullptr;
		QPushButton *m_addNewButton = nullptr;
		QPushButton *m_removeButton = nullptr;
		ThingPropertyEditor *m_propertyEditor = nullptr;

		// Signature fields
		QLineEdit *m_signatureEdit = nullptr;
		QLineEdit *m_signatureHexEdit = nullptr;

		// Per-category list widgets + search list, managed by a QStackedWidget
		QStackedWidget *m_listStack = nullptr;
		std::array<QListWidget *, THING_CATEGORY_COUNT> m_categoryLists {};
		QListWidget *m_searchList = nullptr;
		QListWidget *m_activeList = nullptr;

		// Search debounce
		QTimer *m_searchDebounceTimer = nullptr;
};
