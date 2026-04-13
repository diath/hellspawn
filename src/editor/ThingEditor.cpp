/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/ThingEditor.h"

#include "util/Logger.h"
#include "util/Timing.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <limits>

#include <fmt/core.h>

// ---------------------------------------------------------------------------
// Static clipboard
// ---------------------------------------------------------------------------

std::shared_ptr<ThingType> ThingEditor::s_clipboardThing;

bool ThingEditor::hasClipboardThing()
{
	return s_clipboardThing != nullptr;
}

const ThingType *ThingEditor::clipboardThing()
{
	return s_clipboardThing.get();
}

void ThingEditor::clearClipboardThing()
{
	s_clipboardThing.reset();
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ThingEditor::ThingEditor(QWidget *parent)
: QWidget(parent)
{
	m_iconProvider = new ThingIconProvider(this);
	connect(m_iconProvider, &ThingIconProvider::iconReady, this, &ThingEditor::onAsyncIconReady);

	setupUi();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ThingEditor::setData(SpriteFile *spriteFile, DatFile *datFile, OtbFile *otbFile, ItemsXml *itemsXml)
{
	m_spriteFile = spriteFile;
	m_datFile = datFile;
	m_otbFile = otbFile;
	m_itemsXml = itemsXml;

	// Sprite file pointer changed — update the icon provider and clear stale icons.
	m_iconProvider->setSpriteFile(spriteFile);
	clearIconCache();

	m_propertyEditor->clear();

	// Clear search state
	m_searchEdit->blockSignals(true);
	m_searchEdit->clear();
	m_searchEdit->blockSignals(false);
	m_searchActive = false;
	m_searchDebounceTimer->stop();

	// Update signature fields from the loaded dat file
	updateSignatureFields();

	// Populate all category lists up-front
	populateAllCategoryLists();

	// Show the list for the current category
	int cat = static_cast<int>(currentCategory());
	switchToList(m_categoryLists[cat]);
}

SpriteFile *ThingEditor::spriteFile() const { return m_spriteFile; }
DatFile *ThingEditor::datFile() const { return m_datFile; }
OtbFile *ThingEditor::otbFile() const { return m_otbFile; }
ItemsXml *ThingEditor::itemsXml() const { return m_itemsXml; }

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

QListWidget *ThingEditor::createThingListWidget()
{
	auto *list = new QListWidget;
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
	list->setSpacing(1);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	list->setUniformItemSizes(true);
	connectListSignals(list);
	return list;
}

void ThingEditor::connectListSignals(QListWidget *list)
{
	connect(list, &QListWidget::currentItemChanged, this, &ThingEditor::onThingSelected);
	connect(list, &QWidget::customContextMenuRequested, this, &ThingEditor::onThingContextMenu);
}

void ThingEditor::setupUi()
{
	auto *mainLayout = new QHBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(0);

	m_splitter = new QSplitter(Qt::Horizontal, this);
	mainLayout->addWidget(m_splitter);

	// ---- Left panel: category selector, search, thing list(s) ----
	auto *leftWidget = new QWidget;
	auto *leftLayout = new QVBoxLayout(leftWidget);
	leftLayout->setContentsMargins(0, 0, 4, 0);
	leftLayout->setSpacing(4);

	// Category selector
	auto *categoryLayout = new QHBoxLayout;
	categoryLayout->setSpacing(4);
	categoryLayout->addWidget(new QLabel("Category:"));

	m_categoryCombo = new QComboBox;
	m_categoryCombo->addItem("Items", static_cast<int>(ThingCategory::Item));
	m_categoryCombo->addItem("Creatures", static_cast<int>(ThingCategory::Creature));
	m_categoryCombo->addItem("Effects", static_cast<int>(ThingCategory::Effect));
	m_categoryCombo->addItem("Missiles", static_cast<int>(ThingCategory::Missile));
	m_categoryCombo->setFixedWidth(140);
	connect(m_categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ThingEditor::onCategoryChanged);
	categoryLayout->addWidget(m_categoryCombo);
	categoryLayout->addStretch();

	leftLayout->addLayout(categoryLayout);

	// Search bar
	auto *searchLayout = new QHBoxLayout;
	searchLayout->setSpacing(4);
	searchLayout->addWidget(new QLabel("Search:"));

	m_searchEdit = new QLineEdit;
	m_searchEdit->setPlaceholderText("Filter by ID or name...");
	m_searchEdit->setClearButtonEnabled(true);
	connect(m_searchEdit, &QLineEdit::textChanged, this, &ThingEditor::onSearchTextChanged);
	searchLayout->addWidget(m_searchEdit, 1);

	leftLayout->addLayout(searchLayout);

	// Debounce timer for search (single-shot)
	m_searchDebounceTimer = new QTimer(this);
	m_searchDebounceTimer->setSingleShot(true);
	connect(m_searchDebounceTimer, &QTimer::timeout, this, &ThingEditor::onSearchDebounceTimeout);

	// Stacked widget holding all list widgets
	m_listStack = new QStackedWidget;

	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		m_categoryLists[i] = createThingListWidget();
		m_listStack->addWidget(m_categoryLists[i]);
	}

	m_searchList = createThingListWidget();
	m_listStack->addWidget(m_searchList);

	// Start with the Items category list visible
	m_activeList = m_categoryLists[static_cast<int>(ThingCategory::Item)];
	m_listStack->setCurrentWidget(m_activeList);

	leftLayout->addWidget(m_listStack, 1);

	// Count label and buttons
	auto *bottomLayout = new QHBoxLayout;
	bottomLayout->setSpacing(4);

	m_countLabel = new QLabel("0 items");
	bottomLayout->addWidget(m_countLabel);
	bottomLayout->addStretch();

	m_addNewButton = new QPushButton("Add New");
	m_addNewButton->setFixedWidth(80);
	connect(m_addNewButton, &QPushButton::clicked, this, &ThingEditor::onAddNewClicked);
	bottomLayout->addWidget(m_addNewButton);

	m_removeButton = new QPushButton("Remove");
	m_removeButton->setFixedWidth(70);
	m_removeButton->setEnabled(false);
	connect(m_removeButton, &QPushButton::clicked, this, &ThingEditor::onRemoveClicked);
	bottomLayout->addWidget(m_removeButton);

	leftLayout->addLayout(bottomLayout);

	// Separator between bottom buttons and signature group
	auto *separator = new QFrame;
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	leftLayout->addWidget(separator);

	// Signature group box (below item count / buttons)
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
	connect(m_signatureEdit, &QLineEdit::textChanged, this, &ThingEditor::onSignatureTextChanged);
	signatureFormLayout->addRow("Decimal:", m_signatureEdit);

	m_signatureHexEdit = new QLineEdit;
	m_signatureHexEdit->setReadOnly(true);
	m_signatureHexEdit->setPlaceholderText("0x00000000");
	signatureFormLayout->addRow("Hex:", m_signatureHexEdit);

	leftLayout->addWidget(signatureGroup);

	m_splitter->addWidget(leftWidget);

	// ---- Right panel: property editor ----
	m_propertyEditor = new ThingPropertyEditor;
	connect(m_propertyEditor, &ThingPropertyEditor::thingSpritesModified, this, &ThingEditor::onThingSpriteModified);
	connect(m_propertyEditor, &ThingPropertyEditor::thingModified, this, &ThingEditor::onThingPropertyModified);
	m_splitter->addWidget(m_propertyEditor);

	// Set initial splitter proportions (roughly 1:2)
	m_splitter->setStretchFactor(0, 1);
	m_splitter->setStretchFactor(1, 2);
	m_splitter->setSizes({350, 700});
}

// ---------------------------------------------------------------------------
// List management helpers
// ---------------------------------------------------------------------------

QListWidget *ThingEditor::activeList() const
{
	return m_activeList;
}

void ThingEditor::switchToList(QListWidget *list)
{
	if (m_activeList == list) {
		// Still trigger a selection refresh so the property editor is in sync
		QListWidgetItem *cur = list->currentItem();
		onThingSelected(cur, nullptr);
		return;
	}

	m_activeList = list;
	m_listStack->setCurrentWidget(list);

	// Update count label from the new list
	m_countLabel->setText(QString("%1 %2")
	                          .arg(list->count())
	                          .arg(thingCategoryName(currentCategory())));

	// Trigger selection handler for whatever is selected in the new list
	QListWidgetItem *cur = list->currentItem();
	onThingSelected(cur, nullptr);
}

void ThingEditor::selectItemById(QListWidget *list, uint32_t thingId)
{
	QListWidgetItem *item = findListItem(list, thingId);
	if (item) {
		list->setCurrentItem(item);
		list->scrollToItem(item);
	}
}

// ---------------------------------------------------------------------------
// Targeted list operations
// ---------------------------------------------------------------------------

QListWidgetItem *ThingEditor::findListItem(QListWidget *list, uint32_t thingId) const
{
	// Search backwards — the common case (end-of-array operations, recently
	// added items) is near the bottom of the list, so this is faster on average.
	for (int i = list->count() - 1; i >= 0; --i) {
		QListWidgetItem *item = list->item(i);
		if (item && item->data(Qt::UserRole).toUInt() == thingId)
			return item;
	}
	return nullptr;
}

void ThingEditor::appendThingItem(QListWidget *list, uint16_t thingId, ThingCategory category, const ThingType *thing)
{
	auto *item = new QListWidgetItem;
	updateListItemDisplay(item, thingId, category, thing);
	item->setData(Qt::UserRole, static_cast<uint32_t>(thingId));
	list->addItem(item);
	registerItemInMap(list, thingId, item);
}

void ThingEditor::removeThingItem(QListWidget *list, uint32_t thingId)
{
	// Search backwards — end-of-array removal targets the last item.
	for (int i = list->count() - 1; i >= 0; --i) {
		QListWidgetItem *item = list->item(i);
		if (item && item->data(Qt::UserRole).toUInt() == thingId) {
			unregisterItemFromMap(list, static_cast<uint16_t>(thingId));
			delete list->takeItem(i);
			return;
		}
	}
}

void ThingEditor::updateCountLabel()
{
	if (!m_activeList)
		return;
	int count = m_activeList->count();
	QString catName = thingCategoryName(currentCategory());
	if (m_searchActive)
		m_countLabel->setText(QString("%1 %2 (filtered)").arg(count).arg(catName));
	else
		m_countLabel->setText(QString("%1 %2").arg(count).arg(catName));
}

// ---------------------------------------------------------------------------
// List population
// ---------------------------------------------------------------------------

ThingCategory ThingEditor::currentCategory() const
{
	int idx = m_categoryCombo->currentIndex();
	if (idx < 0)
		return ThingCategory::Item;
	return static_cast<ThingCategory>(m_categoryCombo->currentData().toInt());
}

void ThingEditor::populateAllCategoryLists()
{
	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		populateCategoryList(static_cast<ThingCategory>(i));
	}
}

void ThingEditor::populateCategoryList(ThingCategory category)
{
	int cat = static_cast<int>(category);
	if (cat < 0 || cat >= THING_CATEGORY_COUNT)
		return;

	int64_t startMs = Timing::currentMillis();

	// Clear cached display text for this category so every entry is rebuilt
	// from current ThingType data during repopulation.
	m_displayTextCache[cat].clear();

	// Cancel any pending async icon requests for this category and clear the
	// icon cache so stale entries are not reused.
	m_iconProvider->cancelCategory(category);
	m_iconProvider->invalidateCategory(category);

	QListWidget *list = m_categoryLists[cat];

	m_populatingList = true;

	// Suppress signals and visual updates during bulk insertion.
	// This avoids per-item layout recalculations and spurious
	// currentItemChanged emissions while the list is being rebuilt.
	list->blockSignals(true);
	list->setUpdatesEnabled(false);

	list->clear();
	clearItemMap(list);

	if (!m_datFile || !m_datFile->isLoaded()) {
		list->setUpdatesEnabled(true);
		list->blockSignals(false);
		m_populatingList = false;
		return;
	}

	const auto &things = m_datFile->getThingTypes(category);
	uint16_t firstId = m_datFile->getFirstId(category);

	// Phase 1: Create all list items with display text and placeholder icons.
	// No sprite work happens here — icons are loaded asynchronously.
	const QIcon &placeholder = m_iconProvider->placeholderIcon();

	for (size_t i = firstId; i < things.size(); ++i) {
		const auto &thingPtr = things[i];
		if (!thingPtr)
			continue;

		uint16_t thingId = static_cast<uint16_t>(i);

		auto *item = new QListWidgetItem;
		item->setText(buildDisplayText(thingId, category, thingPtr.get()));
		item->setIcon(placeholder);
		item->setData(Qt::UserRole, static_cast<uint32_t>(thingId));
		list->addItem(item);
		registerItemInMap(list, thingId, item);
	}

	list->setUpdatesEnabled(true);
	list->blockSignals(false);

	// If this is the active list, update the count label
	if (list == m_activeList) {
		m_countLabel->setText(QString("%1 %2")
		                          .arg(list->count())
		                          .arg(thingCategoryName(category)));
	}

	m_populatingList = false;

	int64_t listMs = Timing::currentMillis() - startMs;
	Log().debug("ThingEditor: populated {} list — {} items ({}ms, icons loading async)", thingCategoryName(category), list->count(), listMs);

	// Phase 2: Enqueue async icon requests for every item.  The background
	// worker builds icons off the UI thread; completed icons are delivered
	// via ThingIconProvider::iconReady → onAsyncIconReady which updates the
	// QListWidgetItem in-place.
	for (size_t i = firstId; i < things.size(); ++i) {
		const auto &thingPtr = things[i];
		if (!thingPtr)
			continue;

		uint16_t thingId = static_cast<uint16_t>(i);
		m_iconProvider->requestIcon(category, thingId, thingPtr.get());
	}
}

void ThingEditor::populateSearchList(const QString &searchText)
{
	m_populatingList = true;

	// Suppress signals and visual updates during bulk insertion.
	m_searchList->blockSignals(true);
	m_searchList->setUpdatesEnabled(false);

	m_searchList->clear();
	clearItemMap(m_searchList);

	if (!m_datFile || !m_datFile->isLoaded() || searchText.isEmpty()) {
		m_searchList->setUpdatesEnabled(true);
		m_searchList->blockSignals(false);
		m_populatingList = false;
		return;
	}

	ThingCategory category = currentCategory();
	const auto &things = m_datFile->getThingTypes(category);
	uint16_t firstId = m_datFile->getFirstId(category);
	QString lowerSearch = searchText.toLower();
	const QIcon &placeholder = m_iconProvider->placeholderIcon();

	// Collect matching items — text + placeholder icon only.
	std::vector<std::pair<uint16_t, const ThingType *>> matches;

	for (size_t i = firstId; i < things.size(); ++i) {
		const auto &thingPtr = things[i];
		if (!thingPtr)
			continue;

		uint16_t thingId = static_cast<uint16_t>(i);
		QString displayText = buildDisplayText(thingId, category, thingPtr.get());

		if (!displayText.toLower().contains(lowerSearch)) {
			continue;
		}

		auto *item = new QListWidgetItem;
		item->setText(displayText);
		// Try the provider cache first; fall back to placeholder.
		QIcon cached = m_iconProvider->getCachedIcon(category, thingId);
		item->setIcon(!cached.isNull() ? cached : placeholder);
		item->setData(Qt::UserRole, static_cast<uint32_t>(thingId));
		m_searchList->addItem(item);
		registerItemInMap(m_searchList, thingId, item);

		if (cached.isNull()) {
			matches.push_back({thingId, thingPtr.get()});
		}
	}

	m_searchList->setUpdatesEnabled(true);
	m_searchList->blockSignals(false);

	m_countLabel->setText(QString("%1 %2 (filtered)")
	                          .arg(m_searchList->count())
	                          .arg(thingCategoryName(category)));

	m_populatingList = false;

	// Enqueue async icon requests for items that weren't already cached.
	for (auto &[thingId, thing]: matches) {
		m_iconProvider->requestIcon(category, thingId, thing);
	}
}

void ThingEditor::rebuildCurrentCategoryList()
{
	ThingCategory category = currentCategory();
	populateCategoryList(category);

	// If search is active, also repopulate the search list
	if (m_searchActive) {
		QString searchText = m_searchEdit->text().trimmed();
		populateSearchList(searchText);
	}
}

// ---------------------------------------------------------------------------
// Display text & icon helpers
// ---------------------------------------------------------------------------

void ThingEditor::updateListItemDisplay(QListWidgetItem *item, uint16_t thingId, ThingCategory category, const ThingType *thing)
{
	item->setText(buildDisplayText(thingId, category, thing));

	// Synchronous icon build for single-item updates (edits, resets).
	// The result is inserted into the provider's cache so subsequent
	// lookups (e.g. search list, async delivery) find it immediately.
	QIcon icon = buildThingIconSync(thingId, category, thing);
	item->setIcon(!icon.isNull() ? icon : m_iconProvider->placeholderIcon());
}

QString ThingEditor::buildDisplayText(uint16_t thingId, ThingCategory category, const ThingType *thing) const
{
	// Check the per-category display text cache before doing any work.
	int cat = static_cast<int>(category);
	if (cat >= 0 && cat < THING_CATEGORY_COUNT) {
		auto it = m_displayTextCache[cat].find(thingId);
		if (it != m_displayTextCache[cat].end())
			return it->second;
	}

	// --- Line 1: Thing type label + ID (retain existing pattern) ---
	QString text = QString("%1: #%2").arg(thingCategoryLabel(category)).arg(thingId);

	// For Items, show the SID (server ID) and name via a single OTB lookup
	if (category == ThingCategory::Item) {
		uint16_t serverId = 0;
		QString name;

		// Single OTB lookup to resolve both serverId and item name
		if (m_otbFile && m_otbFile->isLoaded()) {
			const OtbItemType *otbItem = m_otbFile->getItemByClientId(thingId);
			if (otbItem && otbItem->serverId > 0) {
				serverId = otbItem->serverId;
				if (m_itemsXml && m_itemsXml->isLoaded()) {
					std::string stdName = m_itemsXml->getItemName(serverId);
					if (!stdName.empty()) {
						name = QString::fromStdString(stdName);
					}
				}
			}
		}

		if (serverId > 0 && !name.isEmpty()) {
			text += QString(" - (SID: %1) %2").arg(serverId).arg(name);
		} else if (serverId > 0) {
			text += QString(" - (SID: %1)").arg(serverId);
		}
	}

	// --- Line 2: Conditional details built as QStringList, joined with ", " ---
	if (thing) {
		const FrameGroup *fg = thing->getFrameGroup();
		if (fg) {
			QStringList details;

			switch (category) {

				case ThingCategory::Item: {
					// Size: <w>x<h> — only shown for multi-tile items
					if (fg->width > 1 || fg->height > 1) {
						details << QString("Size: %1x%2").arg(fg->width).arg(fg->height);
					}

					// Animated — if the idle/default group has more than one animation phase
					if (fg->animationPhases > 1) {
						details << QStringLiteral("Animated");
					}

					// Append flag labels that are present on the ThingType
					static const std::vector<std::pair<uint8_t, const char *>> itemFlags = {
					    {ThingAttrGroundBorder, "Border"},
					    {ThingAttrContainer, "Container"},
					    {ThingAttrMultiUse, "Multi Use"},
					    {ThingAttrNotWalkable, "Not Walkable"},
					    {ThingAttrNotPathable, "Not Pathable"},
					    {ThingAttrStackable, "Stackable"},
					    {ThingAttrForceUse, "Force Use"},
					    {ThingAttrSplash, "Splash"},
					    {ThingAttrBlockProjectile, "Block Projectile"},
					    {ThingAttrPickupable, "Pickupable"},
					};
					for (const auto &[attr, label]: itemFlags) {
						if (thing->hasAttr(attr)) {
							details << QString::fromLatin1(label);
						}
					}

					// Light Source — if the ThingType has the light attribute
					if (thing->hasAttr(ThingAttrLight)) {
						details << QStringLiteral("Light Source");
					}
					break;
				}

				case ThingCategory::Effect: {
					// Size: <w>x<h> — only shown for multi-tile effects
					if (fg->width > 1 || fg->height > 1) {
						details << QString("Size: %1x%2").arg(fg->width).arg(fg->height);
					}

					// Frames: always show the animation phase count
					details << QString("Frames: %1").arg(fg->animationPhases);

					// Duration: total animation duration in milliseconds (from animator data)
					if (fg->animator) {
						details << QString("Duration: %1ms").arg(fg->animator->totalDuration());
					}
					break;
				}

				case ThingCategory::Missile: {
					// Size: <w>x<h> — only shown for multi-tile missiles
					if (fg->width > 1 || fg->height > 1) {
						details << QString("Size: %1x%2").arg(fg->width).arg(fg->height);
					}
					break;
				}

				case ThingCategory::Creature: {
					// Size: <w>x<h> — only shown for multi-tile creatures
					if (fg->width > 1 || fg->height > 1) {
						details << QString("Size: %1x%2").arg(fg->width).arg(fg->height);
					}

					// Idle Animation — if the idle group has more than one animation phase
					if (fg->animationPhases > 1) {
						details << QStringLiteral("Idle Animation");
					}

					// Addons — if creature has addon patterns beyond the base outfit
					if (fg->patternY > 1) {
						details << QStringLiteral("Addons");
					}

					// Mountable — if creature has mount patterns
					if (fg->patternZ > 1) {
						details << QStringLiteral("Mountable");
					}

					// Colorable — if creature has color layers beyond the base
					if (fg->layers > 1) {
						details << QStringLiteral("Colorable");
					}
					break;
				}

				default:
					break;
			}

			// Append the details as line 2 if any were collected
			if (!details.isEmpty()) {
				text += QStringLiteral("\n") + details.join(QStringLiteral(", "));
			}
		}
	}

	// Store in cache before returning.
	if (cat >= 0 && cat < THING_CATEGORY_COUNT)
		m_displayTextCache[cat][thingId] = text;

	return text;
}

QString ThingEditor::getThingName(uint16_t clientId, ThingCategory category) const
{
	if (category == ThingCategory::Item && m_otbFile && m_otbFile->isLoaded() && m_itemsXml && m_itemsXml->isLoaded()) {
		const OtbItemType *otbItem = m_otbFile->getItemByClientId(clientId);
		if (otbItem && otbItem->serverId > 0) {
			std::string name = m_itemsXml->getItemName(otbItem->serverId);
			if (!name.empty()) {
				return QString::fromStdString(name);
			}
		}
	}
	return {};
}

QIcon ThingEditor::buildThingIcon(uint16_t thingId, ThingCategory category, const ThingType *thing) const
{
	if (!thing || !m_spriteFile) {
		return {};
	}

	// Check the provider's LRU cache before doing any compositing work.
	QIcon cached = m_iconProvider->getCachedIcon(category, thingId);
	if (!cached.isNull())
		return cached;

	// Not cached — request async build.  Returns null; the caller should
	// use the placeholder icon until iconReady() fires.
	return m_iconProvider->requestIcon(category, thingId, thing);
}

QIcon ThingEditor::buildThingIconSync(uint16_t thingId, ThingCategory category, const ThingType *thing) const
{
	// Synchronous icon build — used for single-item updates after edits.
	// Reads from SpriteFile::getSpriteImage() (which checks overrides),
	// then inserts the result into the provider's cache.
	if (!thing || !m_spriteFile) {
		return {};
	}

	const FrameGroup *fg = thing->getFrameGroup();
	if (!fg || fg->spriteIds.empty()) {
		return {};
	}

	int w = fg->width;
	int h = fg->height;

	int baseSpriteOffset = 0;
	if (thing->category == ThingCategory::Creature && fg->patternX >= 3) {
		baseSpriteOffset = 2 * w * h * static_cast<int>(fg->layers);
	}

	int nativeW = w * SPRITE_SIZE;
	int nativeH = h * SPRITE_SIZE;

	QImage composite(nativeW, nativeH, QImage::Format_ARGB32);
	composite.fill(Qt::transparent);

	bool anyPixel = false;
	{
		QPainter painter(&composite);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

		for (int ty = 0; ty < h; ++ty) {
			for (int tx = 0; tx < w; ++tx) {
				int index = baseSpriteOffset + ty * w + tx;
				if (index >= static_cast<int>(fg->spriteIds.size()))
					continue;

				uint32_t spriteId = fg->spriteIds[index];
				if (spriteId == 0)
					continue;

				// Main-thread path: uses getSpriteImage which checks overrides.
				QImage spriteImg = m_spriteFile->getSpriteImage(spriteId);
				if (spriteImg.isNull())
					continue;

				int drawX = (w - tx - 1) * SPRITE_SIZE;
				int drawY = (h - ty - 1) * SPRITE_SIZE;
				painter.drawImage(drawX, drawY, spriteImg);
				anyPixel = true;
			}
		}
		painter.end();
	}

	if (!anyPixel) {
		return {};
	}

	QImage canvas(ICON_SIZE, ICON_SIZE, QImage::Format_ARGB32);
	canvas.fill(Qt::transparent);

	if (w > 2 || h > 2) {
		QImage scaled = composite.scaled(ICON_SIZE, ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		int xOff = (ICON_SIZE - scaled.width()) / 2;
		int yOff = (ICON_SIZE - scaled.height()) / 2;

		QPainter p(&canvas);
		p.drawImage(xOff, yOff, scaled);
		p.end();
	} else {
		int xOff = (ICON_SIZE - nativeW) / 2;
		int yOff = (ICON_SIZE - nativeH) / 2;

		QPainter p(&canvas);
		p.drawImage(xOff, yOff, composite);
		p.end();
	}

	QIcon result(QPixmap::fromImage(canvas));

	// Insert into the provider's cache so async lookups find it immediately.
	m_iconProvider->insertIcon(category, thingId, result);

	return result;
}

// ---------------------------------------------------------------------------
// Category change
// ---------------------------------------------------------------------------

void ThingEditor::onCategoryChanged(int /*index*/)
{
	m_currentCategory = currentCategory();
	m_propertyEditor->clear();
	m_removeButton->setEnabled(false);

	// If search is active, repopulate the search list for the new category
	if (m_searchActive) {
		QString searchText = m_searchEdit->text().trimmed();
		populateSearchList(searchText);
		// Stay on the search list — just refreshed its contents
		m_countLabel->setText(QString("%1 %2 (filtered)")
		                          .arg(m_searchList->count())
		                          .arg(thingCategoryName(m_currentCategory)));
		// Trigger selection refresh (search list was cleared and repopulated)
		QListWidgetItem *cur = m_searchList->currentItem();
		onThingSelected(cur, nullptr);
	} else {
		// Switch to the appropriate category list (already populated)
		int cat = static_cast<int>(m_currentCategory);
		switchToList(m_categoryLists[cat]);
	}
}

// ---------------------------------------------------------------------------
// Search with debounce
// ---------------------------------------------------------------------------

void ThingEditor::onSearchTextChanged(const QString &text)
{
	QString trimmed = text.trimmed();

	// Always cancel any pending debounce
	m_searchDebounceTimer->stop();

	if (trimmed.isEmpty()) {
		// Search cleared — switch back to category list
		m_searchActive = false;
		int cat = static_cast<int>(currentCategory());
		switchToList(m_categoryLists[cat]);
		return;
	}

	// Search is now active
	m_searchActive = true;

	if (trimmed.length() <= 2) {
		// Short query: debounce at 100ms
		m_searchDebounceTimer->start(100);
	} else {
		// 3+ characters: update immediately
		populateSearchList(trimmed);
		switchToList(m_searchList);
	}
}

void ThingEditor::onSearchDebounceTimeout()
{
	QString trimmed = m_searchEdit->text().trimmed();
	if (trimmed.isEmpty()) {
		// User cleared during debounce — switch back
		m_searchActive = false;
		int cat = static_cast<int>(currentCategory());
		switchToList(m_categoryLists[cat]);
		return;
	}

	populateSearchList(trimmed);
	switchToList(m_searchList);
}

// ---------------------------------------------------------------------------
// Selection handling
// ---------------------------------------------------------------------------

void ThingEditor::onThingSelected(QListWidgetItem *current, QListWidgetItem * /*previous*/)
{
	if (m_populatingList)
		return;

	if (!current || !m_datFile) {
		m_propertyEditor->clear();
		m_removeButton->setEnabled(false);
		return;
	}

	uint32_t thingId = current->data(Qt::UserRole).toUInt();
	ThingCategory category = currentCategory();

	ThingType *thing = m_datFile->getThingTypeMut(static_cast<uint16_t>(thingId), category);
	if (!thing) {
		m_propertyEditor->clear();
		m_removeButton->setEnabled(false);
		return;
	}

	m_propertyEditor->setThingType(thing, m_spriteFile);
	m_removeButton->setEnabled(true);
}

// ---------------------------------------------------------------------------
// Property modification refresh
// ---------------------------------------------------------------------------

void ThingEditor::onThingSpriteModified()
{
	// Invalidate the icon cache entry for the currently selected item.
	// This runs BEFORE onThingPropertyModified (which is connected to
	// thingModified), so when updateListItemDisplay calls buildThingIcon
	// the stale cache entry will already be gone, forcing a rebuild.
	QListWidgetItem *currentItem = m_activeList->currentItem();
	if (!currentItem || !m_datFile)
		return;

	uint32_t thingId = currentItem->data(Qt::UserRole).toUInt();
	ThingCategory category = currentCategory();
	invalidateIcon(category, static_cast<uint16_t>(thingId));
}

void ThingEditor::onThingPropertyModified()
{
	emit dataModified();

	QListWidgetItem *currentItem = m_activeList->currentItem();
	if (!currentItem || !m_datFile)
		return;

	uint32_t thingId = currentItem->data(Qt::UserRole).toUInt();
	ThingCategory category = currentCategory();

	const ThingType *thing = m_datFile->getThingType(static_cast<uint16_t>(thingId), category);
	if (!thing)
		return;

	// Invalidate the cached display text for this single entry so
	// updateListItemDisplay() rebuilds it from the modified ThingType.
	invalidateDisplayText(category, static_cast<uint16_t>(thingId));

	// Update the item in the active list
	updateListItemDisplay(currentItem, static_cast<uint16_t>(thingId), category, thing);

	// If search is active, also update the matching item in the category list to keep it in sync
	if (m_searchActive) {
		int cat = static_cast<int>(category);
		QListWidgetItem *catItem = findListItem(m_categoryLists[cat], thingId);
		if (catItem)
			updateListItemDisplay(catItem, static_cast<uint16_t>(thingId), category, thing);
	}
}

// ---------------------------------------------------------------------------
// Add new thing
// ---------------------------------------------------------------------------

void ThingEditor::onAddNewClicked()
{
	if (!m_datFile || !m_datFile->isLoaded()) {
		QMessageBox::warning(this, "Add Thing", "No data file is loaded.");
		return;
	}

	ThingCategory category = currentCategory();

	bool useClipboard = false;

	// When the clipboard contains a ThingType, show a modal dialog to let the
	// user choose between creating an empty thing or one from the clipboard.
	if (hasClipboardThing()) {
		QDialog dialog(this);
		dialog.setWindowTitle("Add Thing");
		dialog.setModal(true);

		auto *dialogLayout = new QVBoxLayout(&dialog);

		auto *label = new QLabel("Choose what to add:");
		dialogLayout->addWidget(label);

		auto *newThingRadio = new QRadioButton("New Thing");
		auto *clipboardThingRadio = new QRadioButton("Clipboard Thing");
		newThingRadio->setChecked(true);

		auto *radioGroup = new QButtonGroup(&dialog);
		radioGroup->addButton(newThingRadio, 0);
		radioGroup->addButton(clipboardThingRadio, 1);

		dialogLayout->addWidget(newThingRadio);
		dialogLayout->addWidget(clipboardThingRadio);

		auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		dialogLayout->addWidget(buttonBox);

		if (dialog.exec() != QDialog::Accepted)
			return;

		useClipboard = (radioGroup->checkedId() == 1);
	}

	std::shared_ptr<ThingType> newThing;

	if (useClipboard && s_clipboardThing) {
		// Deep copy from clipboard
		newThing = std::make_shared<ThingType>(*s_clipboardThing);
		newThing->category = category;
	} else {
		// Create a blank ThingType with a single default frame group
		newThing = std::make_shared<ThingType>();
		newThing->category = category;

		auto defaultGroup = std::make_unique<FrameGroup>();
		defaultGroup->type = FrameGroupType::Default;
		defaultGroup->width = 1;
		defaultGroup->height = 1;
		defaultGroup->exactSize = 32;
		defaultGroup->layers = 1;
		defaultGroup->patternX = 1;
		defaultGroup->patternY = 1;
		defaultGroup->patternZ = 1;
		defaultGroup->animationPhases = 1;
		defaultGroup->spriteIds.resize(1, 0);
		newThing->frameGroups[0] = std::move(defaultGroup);
	}

	uint16_t newId = m_datFile->addThingType(category, newThing);
	newThing->id = newId;

	Log().info("ThingEditor: added new {} with ID {}{}", thingCategoryName(category), newId, useClipboard ? " (from clipboard)" : "");

	// Targeted update: append a single item to the category list instead of
	// rebuilding the entire list. The new ID is always at the end of the array.
	int cat = static_cast<int>(category);
	appendThingItem(m_categoryLists[cat], newId, category, newThing.get());

	// Clear search and switch to category list so the user sees the new item
	if (m_searchActive) {
		m_searchEdit->blockSignals(true);
		m_searchEdit->clear();
		m_searchEdit->blockSignals(false);
		m_searchActive = false;
		m_searchDebounceTimer->stop();
	}

	switchToList(m_categoryLists[cat]);
	selectItemById(m_categoryLists[cat], newId);

	emit dataModified();
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void ThingEditor::onThingContextMenu(const QPoint &pos)
{
	QListWidget *list = m_activeList;
	QListWidgetItem *listItem = list->itemAt(pos);
	if (!listItem || !m_datFile || !m_datFile->isLoaded())
		return;

	uint32_t thingId = listItem->data(Qt::UserRole).toUInt();
	ThingCategory category = currentCategory();

	QMenu menu(this);

	// "Duplicate Thing" — available for all categories
	QAction *duplicateAction = menu.addAction("Duplicate Thing");
	connect(duplicateAction, &QAction::triggered, this, [this, thingId, category]() {
		duplicateThing(static_cast<uint16_t>(thingId), category);
	});

	// "Copy Thing" — copies the ThingType into the static clipboard
	QAction *copyThingAction = menu.addAction("Copy Thing");
	connect(copyThingAction, &QAction::triggered, this, [this, thingId, category]() {
		copyThing(static_cast<uint16_t>(thingId), category);
	});

	menu.addSeparator();

	if (category == ThingCategory::Item) {
		// Item ThingTypes: show both Copy Server ID and Copy Client ID
		QAction *copyCidAction = menu.addAction("Copy Client ID");
		connect(copyCidAction, &QAction::triggered, this, [thingId]() {
			QApplication::clipboard()->setText(QString::number(thingId));
		});

		QAction *copySidAction = menu.addAction("Copy Server ID");
		uint16_t serverId = 0;
		if (m_otbFile && m_otbFile->isLoaded()) {
			const OtbItemType *otbItem = m_otbFile->getItemByClientId(static_cast<uint16_t>(thingId));
			if (otbItem) {
				serverId = otbItem->serverId;
			}
		}
		if (serverId > 0) {
			uint16_t sid = serverId;
			connect(copySidAction, &QAction::triggered, this, [sid]() {
				QApplication::clipboard()->setText(QString::number(sid));
			});
		} else {
			copySidAction->setEnabled(false);
			copySidAction->setText("Copy Server ID (N/A)");
		}
	} else {
		// Non-item ThingTypes: show "Copy ID"
		QAction *copyIdAction = menu.addAction("Copy ID");
		connect(copyIdAction, &QAction::triggered, this, [thingId]() {
			QApplication::clipboard()->setText(QString::number(thingId));
		});
	}

	menu.exec(list->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Duplicate thing
// ---------------------------------------------------------------------------

void ThingEditor::duplicateThing(uint16_t thingId, ThingCategory category)
{
	if (!m_datFile || !m_datFile->isLoaded())
		return;

	const ThingType *source = m_datFile->getThingType(thingId, category);
	if (!source) {
		QMessageBox::warning(this, "Duplicate Thing", QString("Could not find %1 #%2 to duplicate.").arg(thingCategoryLabel(category)).arg(thingId));
		return;
	}

	// Deep copy using ThingType's copy constructor
	auto newThing = std::make_shared<ThingType>(*source);

	uint16_t newId = m_datFile->addThingType(category, newThing);
	newThing->id = newId;

	Log().info("ThingEditor: duplicated {} #{} -> new ID {}", thingCategoryLabel(category), thingId, newId);

	// Targeted update: append a single item to the category list instead of
	// rebuilding the entire list. The duplicate gets the next available ID
	// which is always appended at the end of the array.
	int cat = static_cast<int>(category);
	appendThingItem(m_categoryLists[cat], newId, category, newThing.get());

	// Clear search and switch to category list so the user sees the duplicate
	if (m_searchActive) {
		m_searchEdit->blockSignals(true);
		m_searchEdit->clear();
		m_searchEdit->blockSignals(false);
		m_searchActive = false;
		m_searchDebounceTimer->stop();
	}

	switchToList(m_categoryLists[cat]);
	selectItemById(m_categoryLists[cat], newId);

	emit dataModified();
}

void ThingEditor::copyThing(uint16_t thingId, ThingCategory category)
{
	if (!m_datFile || !m_datFile->isLoaded())
		return;

	const ThingType *source = m_datFile->getThingType(thingId, category);
	if (!source) {
		QMessageBox::warning(this, "Copy Thing", QString("Could not find %1 #%2 to copy.").arg(thingCategoryLabel(category)).arg(thingId));
		return;
	}

	// Deep copy into the static clipboard
	s_clipboardThing = std::make_shared<ThingType>(*source);

	Log().info("ThingEditor: copied {} #{} to clipboard", thingCategoryLabel(category), thingId);
}

// ---------------------------------------------------------------------------
// Remove thing
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Display text cache
// ---------------------------------------------------------------------------

void ThingEditor::clearDisplayTextCache()
{
	for (auto &map: m_displayTextCache)
		map.clear();
}

void ThingEditor::invalidateDisplayText(ThingCategory category, uint16_t thingId)
{
	int cat = static_cast<int>(category);
	if (cat >= 0 && cat < THING_CATEGORY_COUNT)
		m_displayTextCache[cat].erase(thingId);
}

void ThingEditor::clearIconCache()
{
	m_iconProvider->cancelAll();
	m_iconProvider->invalidateAll();
}

void ThingEditor::invalidateIcon(ThingCategory category, uint16_t thingId)
{
	m_iconProvider->invalidate(category, thingId);
}

// ---------------------------------------------------------------------------
// Async icon delivery
// ---------------------------------------------------------------------------

void ThingEditor::onAsyncIconReady(ThingCategory category, uint16_t thingId, QIcon icon)
{
	// Update the item in the matching category list (O(1) via reverse map).
	int cat = static_cast<int>(category);
	if (cat >= 0 && cat < THING_CATEGORY_COUNT) {
		QListWidgetItem *catItem = findItemInMap(m_categoryLists[cat], thingId);
		if (catItem) {
			catItem->setIcon(icon);
		}
	}

	// If search is active, also update the matching item in the search list.
	if (m_searchActive) {
		QListWidgetItem *searchItem = findItemInMap(m_searchList, thingId);
		if (searchItem) {
			searchItem->setIcon(icon);
		}
	}
}

// ---------------------------------------------------------------------------
// Reverse lookup helpers
// ---------------------------------------------------------------------------

void ThingEditor::registerItemInMap(QListWidget *list, uint16_t thingId, QListWidgetItem *item)
{
	if (list == m_searchList) {
		m_searchItemMap[thingId] = item;
	} else {
		for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
			if (list == m_categoryLists[i]) {
				m_categoryItemMap[i][thingId] = item;
				return;
			}
		}
	}
}

void ThingEditor::unregisterItemFromMap(QListWidget *list, uint16_t thingId)
{
	if (list == m_searchList) {
		m_searchItemMap.erase(thingId);
	} else {
		for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
			if (list == m_categoryLists[i]) {
				m_categoryItemMap[i].erase(thingId);
				return;
			}
		}
	}
}

void ThingEditor::clearItemMap(QListWidget *list)
{
	if (list == m_searchList) {
		m_searchItemMap.clear();
	} else {
		for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
			if (list == m_categoryLists[i]) {
				m_categoryItemMap[i].clear();
				return;
			}
		}
	}
}

QListWidgetItem *ThingEditor::findItemInMap(QListWidget *list, uint16_t thingId) const
{
	if (list == m_searchList) {
		auto it = m_searchItemMap.find(thingId);
		return (it != m_searchItemMap.end()) ? it->second : nullptr;
	}
	for (int i = 0; i < THING_CATEGORY_COUNT; ++i) {
		if (list == m_categoryLists[i]) {
			auto it = m_categoryItemMap[i].find(thingId);
			return (it != m_categoryItemMap[i].end()) ? it->second : nullptr;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Signature helpers
// ---------------------------------------------------------------------------

void ThingEditor::updateSignatureFields()
{
	if (!m_datFile || !m_datFile->isLoaded()) {
		m_signatureEdit->clear();
		m_signatureHexEdit->clear();
		return;
	}

	// Block signals to avoid recursive updates while populating
	QSignalBlocker blocker(m_signatureEdit);
	m_signatureEdit->setText(QString::number(m_datFile->signature()));
	m_signatureHexEdit->setText(
	    QString("0x%1").arg(m_datFile->signature(), 8, 16, QChar('0'))
	);
}

void ThingEditor::onSignatureTextChanged(const QString &text)
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

	if (m_datFile && m_datFile->isLoaded()) {
		m_datFile->setSignature(sig);
		emit dataModified();
	}
}

void ThingEditor::onRemoveClicked()
{
	if (!m_datFile || !m_datFile->isLoaded())
		return;

	QListWidgetItem *currentItem = m_activeList->currentItem();
	if (!currentItem)
		return;

	uint32_t thingId = currentItem->data(Qt::UserRole).toUInt();
	ThingCategory category = currentCategory();

	// Determine whether this is the last element in the array.
	// End-of-array removal will shrink the vector (element disappears),
	// while mid-array removal resets the element to defaults (element stays).
	bool isLastElement = (thingId == static_cast<uint32_t>(m_datFile->getMaxId(category)));

	QString action = isLastElement ? "remove" : "reset to defaults";
	QMessageBox::StandardButton result = QMessageBox::question(this, "Remove Thing", QString("Are you sure you want to %1 %2 #%3?").arg(action).arg(thingCategoryName(category)).arg(thingId), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (result != QMessageBox::Yes)
		return;

	if (!m_datFile->removeThingType(static_cast<uint16_t>(thingId), category)) {
		QMessageBox::warning(this, "Remove Thing", QString("Failed to remove %1 #%2.").arg(thingCategoryName(category)).arg(thingId));
		return;
	}

	if (isLastElement) {
		// End-of-array: the element was removed and the vector was downsized.
		// Targeted update: remove just the one item from each visible list
		// instead of rebuilding the entire list.
		Log().info("ThingEditor: removed {} #{} (end-of-array, array downsized)", thingCategoryName(category), thingId);

		m_propertyEditor->clear();
		m_removeButton->setEnabled(false);

		// Clean up the stale cache entries for the removed thing.
		invalidateDisplayText(category, static_cast<uint16_t>(thingId));
		invalidateIcon(category, static_cast<uint16_t>(thingId));

		int cat = static_cast<int>(category);
		removeThingItem(m_categoryLists[cat], thingId);

		if (m_searchActive) {
			removeThingItem(m_searchList, thingId);
		}

		updateCountLabel();

		// Trigger selection refresh for whatever item is now current
		QListWidgetItem *cur = m_activeList->currentItem();
		onThingSelected(cur, nullptr);
	} else {
		// Mid-array: the element was reset to default-constructed state.
		// Targeted update: refresh display of just this one item in each
		// visible list instead of rebuilding the entire list.
		Log().info("ThingEditor: reset {} #{} to defaults (mid-array, array size preserved)", thingCategoryName(category), thingId);

		// Invalidate cached display text and icon so the item reflects its new default state.
		invalidateDisplayText(category, static_cast<uint16_t>(thingId));
		invalidateIcon(category, static_cast<uint16_t>(thingId));

		const ThingType *resetThing = m_datFile->getThingType(static_cast<uint16_t>(thingId), category);

		int cat = static_cast<int>(category);
		QListWidgetItem *catItem = findListItem(m_categoryLists[cat], thingId);
		if (catItem && resetThing)
			updateListItemDisplay(catItem, static_cast<uint16_t>(thingId), category, resetThing);

		if (m_searchActive) {
			QListWidgetItem *searchItem = findListItem(m_searchList, thingId);
			if (searchItem && resetThing)
				updateListItemDisplay(searchItem, static_cast<uint16_t>(thingId), category, resetThing);
		}

		// Reselect to refresh the property editor with the reset data
		selectItemById(m_activeList, thingId);
	}

	emit dataModified();
}
