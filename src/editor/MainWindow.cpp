/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/MainWindow.h"
#include "editor/EditorTab.h"
#include "editor/ThingEditor.h"
#include "gitinfo.h"
#include "util/Logger.h"

#include "core/ThingData.h"
#include <QApplication>
#include <QFileDialog>
#include <QStatusBar>
#include <QString>

#include <unordered_set>

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
{
	setWindowTitle("Hellspawn Editor");

	QSize minimumSize(1340, 800);
	resize(minimumSize);
	setMinimumSize(minimumSize);

	setupTabWidget();
	setupMenuBar();

	statusBar()->showMessage("Ready");

	// Start with one empty tab
	newTab();
}

void MainWindow::setupTabWidget()
{
	m_tabWidget = new QTabWidget(this);
	m_tabWidget->setTabsClosable(true);
	m_tabWidget->setMovable(true);
	m_tabWidget->setDocumentMode(true);

	connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
	connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

	setCentralWidget(m_tabWidget);
}

void MainWindow::setupMenuBar()
{
	// File menu
	m_fileMenu = menuBar()->addMenu("&File");

	m_newTabAction = new QAction("&New Tab", this);
	m_newTabAction->setShortcut(QKeySequence::AddTab);
	m_newTabAction->setStatusTip("Create a new editor tab");
	connect(m_newTabAction, &QAction::triggered, this, &MainWindow::newTab);
	m_fileMenu->addAction(m_newTabAction);

	m_fileMenu->addSeparator();

	m_saveAction = new QAction("&Save", this);
	m_saveAction->setShortcut(QKeySequence::Save);
	m_saveAction->setStatusTip("Save current files");
	connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
	m_fileMenu->addAction(m_saveAction);

	m_saveAsAction = new QAction("Save &As...", this);
	m_saveAsAction->setShortcut(QKeySequence::SaveAs);
	m_saveAsAction->setStatusTip("Save current files to a new location");
	connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveCurrentTabAs);
	m_fileMenu->addAction(m_saveAsAction);

	m_fileMenu->addSeparator();

	m_closeTabAction = new QAction("&Close Tab", this);
	m_closeTabAction->setShortcut(QKeySequence::Close);
	m_closeTabAction->setStatusTip("Close the current editor tab");
	connect(m_closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);
	m_fileMenu->addAction(m_closeTabAction);

	m_fileMenu->addSeparator();

	m_exitAction = new QAction("E&xit", this);
	m_exitAction->setShortcut(QKeySequence::Quit);
	m_exitAction->setStatusTip("Exit the application");
	connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
	m_fileMenu->addAction(m_exitAction);

	// Actions menu
	m_actionsMenu = menuBar()->addMenu("&Actions");

	m_clearMarketDataAction = new QAction("Clear &Market Data", this);
	m_clearMarketDataAction->setStatusTip("Remove market data from all items");
	connect(m_clearMarketDataAction, &QAction::triggered, this, &MainWindow::clearMarketData);
	m_actionsMenu->addAction(m_clearMarketDataAction);

	m_removeUnusedSpritesAction = new QAction("Remove &Unused Sprites", this);
	m_removeUnusedSpritesAction->setStatusTip("Remove all sprites not used by any thing");
	connect(m_removeUnusedSpritesAction, &QAction::triggered, this, &MainWindow::removeUnusedSprites);
	m_actionsMenu->addAction(m_removeUnusedSpritesAction);

	m_actionsMenu->addSeparator();

	m_clearClipboardThingAction = new QAction("Clear Clipboard Thing", this);
	m_clearClipboardThingAction->setStatusTip("Clear the copied Thing from the clipboard");
	m_clearClipboardThingAction->setEnabled(ThingEditor::hasClipboardThing());
	connect(m_clearClipboardThingAction, &QAction::triggered, this, []() {
		ThingEditor::clearClipboardThing();
		Log().info("MainWindow: cleared clipboard thing");
	});
	m_actionsMenu->addAction(m_clearClipboardThingAction);

	// Dynamically update the enabled state each time the menu is shown
	connect(m_actionsMenu, &QMenu::aboutToShow, this, [this]() {
		EditorTab *tab = currentEditorTab();
		bool loaded = tab && tab->isLoaded();
		m_clearMarketDataAction->setEnabled(loaded);
		m_removeUnusedSpritesAction->setEnabled(loaded);
		m_clearClipboardThingAction->setEnabled(ThingEditor::hasClipboardThing());
	});

	// Help menu
	m_helpMenu = menuBar()->addMenu("&Help");

	m_aboutAction = new QAction("&About", this);
	m_aboutAction->setStatusTip("About Hellspawn Editor");
	connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
	m_helpMenu->addAction(m_aboutAction);
}

EditorTab *MainWindow::currentEditorTab() const
{
	return qobject_cast<EditorTab *>(m_tabWidget->currentWidget());
}

EditorTab *MainWindow::editorTabAt(int index) const
{
	return qobject_cast<EditorTab *>(m_tabWidget->widget(index));
}

int MainWindow::editorTabCount() const
{
	return m_tabWidget->count();
}

void MainWindow::newTab()
{
	auto *tab = new EditorTab(this);
	int index = m_tabWidget->addTab(tab, "New Tab");
	m_tabWidget->setCurrentIndex(index);

	connect(tab, &EditorTab::dirtyChanged, this, [this, tab](bool dirty) {
		int idx = m_tabWidget->indexOf(tab);
		if (idx >= 0) {
			onTabDirtyChanged(idx, dirty);
		}
	});

	connect(tab, &EditorTab::titleChanged, this, [this, tab]() {
		int idx = m_tabWidget->indexOf(tab);
		if (idx >= 0) {
			updateTabTitle(idx);
		}
	});

	// Trigger onTabChanged manually to update Editor states.
	connect(tab, &EditorTab::loadStateChanged, this, [this, tab](bool /*loaded*/) {
		if (m_tabWidget->currentWidget() == tab) {
			onTabChanged(m_tabWidget->currentIndex());
		}
	});

	updateTabTitle(index);
	updateWindowTitle();

	Log().debug("MainWindow: created new editor tab (index={})", index);
}

void MainWindow::closeTab(int index)
{
	if (index < 0 || index >= m_tabWidget->count()) {
		return;
	}

	if (!confirmCloseTab(index)) {
		return;
	}

	QWidget *widget = m_tabWidget->widget(index);
	m_tabWidget->removeTab(index);
	widget->deleteLater();

	updateWindowTitle();

	Log().info("MainWindow: closed editor tab (index={})", index);
}

void MainWindow::closeCurrentTab()
{
	int index = m_tabWidget->currentIndex();
	if (index >= 0) {
		closeTab(index);
	}
}

void MainWindow::saveCurrentTab()
{
	EditorTab *tab = currentEditorTab();
	if (tab) {
		tab->save();
	}
}

void MainWindow::saveCurrentTabAs()
{
	EditorTab *tab = currentEditorTab();
	if (tab) {
		tab->saveAs();
	}
}

void MainWindow::onTabChanged(int index)
{
	(void)index;
	updateWindowTitle();

	EditorTab *tab = currentEditorTab();
	bool hasTab = (tab != nullptr);
	bool isLoaded = hasTab && tab->isLoaded();

	m_saveAction->setEnabled(isLoaded);
	m_saveAsAction->setEnabled(isLoaded);
	m_closeTabAction->setEnabled(hasTab);
}

void MainWindow::onTabDirtyChanged(int tabIndex, bool /*dirty*/)
{
	updateTabTitle(tabIndex);
	if (tabIndex == m_tabWidget->currentIndex()) {
		updateWindowTitle();
	}
}

void MainWindow::updateTabTitle(int tabIndex)
{
	EditorTab *tab = editorTabAt(tabIndex);
	if (!tab) {
		return;
	}

	QString title = tab->tabTitle();
	if (tab->isDirty()) {
		title = "* " + title;
	}

	m_tabWidget->setTabText(tabIndex, title);
	m_tabWidget->setTabToolTip(tabIndex, tab->tabToolTip());
}

void MainWindow::updateWindowTitle()
{
	EditorTab *tab = currentEditorTab();
	if (tab) {
		QString title = tab->tabTitle();
		if (tab->isDirty()) {
			title = "* " + title;
		}
		setWindowTitle(title + " - Hellspawn Editor");
	} else {
		setWindowTitle("Hellspawn Editor");
	}
}

bool MainWindow::confirmCloseTab(int index)
{
	EditorTab *tab = editorTabAt(index);
	if (!tab || !tab->isDirty()) {
		return true;
	}

	m_tabWidget->setCurrentIndex(index);

	QMessageBox::StandardButton result = QMessageBox::question(
	    this, "Unsaved Changes",
	    QString("The tab \"%1\" has unsaved changes.\n\nDo you want to save before closing?")
	        .arg(tab->tabTitle()),
	    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
	    QMessageBox::Save
	);

	if (result == QMessageBox::Save) {
		tab->save();
		return !tab->isDirty();
	} else if (result == QMessageBox::Discard) {
		return true;
	}

	return false;
}

bool MainWindow::confirmCloseAll()
{
	for (int i = 0; i < m_tabWidget->count(); ++i) {
		if (!confirmCloseTab(i)) {
			return false;
		}
	}
	return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (confirmCloseAll()) {
		event->accept();
	} else {
		event->ignore();
	}
}

void MainWindow::showAbout()
{
	const QString buildId = QStringLiteral(HELLSPAWN_CI_BUILD_ID);
	const QString commitHash = QStringLiteral(HELLSPAWN_COMMIT_HASH);
	const QString repoBase = QStringLiteral("https://github.com/diath/hellspawn");

	// When running a developer build the values are the defaults ("0" and
	// "<Developer Version>") — show them as plain text.  Otherwise render
	// clickable links that point to the CI run and the exact commit.
	QString buildIdHtml;
	if (buildId == QLatin1String("0")) {
		buildIdHtml = buildId;
	} else {
		buildIdHtml = QStringLiteral("<a href=\"%1/actions/runs/%2\">%2</a>")
		                  .arg(repoBase, buildId);
	}

	QString commitHtml;
	if (commitHash == QLatin1String("Developer Version")) {
		commitHtml = commitHash;
	} else {
		const QString shortHash = commitHash.left(8);
		commitHtml = QStringLiteral("<a href=\"%1/commit/%2\">%3</a>")
		                 .arg(repoBase, commitHash, shortHash);
	}

	QMessageBox::about(this, "About Hellspawn Editor", QStringLiteral("<h3>Hellspawn Editor</h3>"
	                                                                  "<p>Data editor for <a href=\"https://github.com/otland/forgottenserver/\">forgottenserver</a> "
	                                                                  "and <a href=\"https://github.com/edubart/otclient/\">otclient</a>.<br>"
	                                                                  "Official Repository: <a href=\"%1\">github.com/diath/hellspawn</a><br>"
	                                                                  "Build ID: %2<br>"
	                                                                  "Commit: %3</p>")
	                                                       .arg(repoBase, buildIdHtml, commitHtml));
}

void MainWindow::clearMarketData()
{
	EditorTab *tab = currentEditorTab();
	if (!tab || !tab->isLoaded() || !tab->datFile().isLoaded()) {
		QMessageBox::warning(this, "Clear Market Data", "No data is currently loaded. Please load files first.");
		return;
	}

	QMessageBox::StandardButton result = QMessageBox::question(
	    this, "Clear Market Data",
	    "Are you sure you want to clear market data from all items?",
	    QMessageBox::Yes | QMessageBox::No,
	    QMessageBox::No
	);

	if (result != QMessageBox::Yes) {
		return;
	}

	DatFile &dat = tab->datFile();
	const auto &things = dat.getThingTypes(ThingCategory::Item);
	uint16_t firstId = dat.getFirstId(ThingCategory::Item);
	int clearedCount = 0;

	for (size_t i = firstId; i < things.size(); ++i) {
		ThingType *thing = dat.getThingTypeMut(static_cast<uint16_t>(i), ThingCategory::Item);
		if (thing && thing->hasAttr(ThingAttrMarket)) {
			thing->removeAttr(ThingAttrMarket);
			++clearedCount;
		}
	}

	if (clearedCount > 0) {
		tab->markDirty();
	}

	QMessageBox::information(this, "Clear Market Data", QString("Market data removed from %1 item(s).").arg(clearedCount));

	Log().info("MainWindow: cleared market data from {} items", clearedCount);
}

void MainWindow::removeUnusedSprites()
{
	EditorTab *tab = currentEditorTab();
	if (!tab || !tab->isLoaded() || !tab->datFile().isLoaded() || !tab->spriteFile().isLoaded()) {
		QMessageBox::warning(this, "Remove Unused Sprites", "No data is currently loaded. Please load files first.");
		return;
	}

	QMessageBox::StandardButton result = QMessageBox::question(
	    this, "Remove Unused Sprites",
	    "Are you sure you want to remove all unused sprites?",
	    QMessageBox::Yes | QMessageBox::No,
	    QMessageBox::No
	);

	if (result != QMessageBox::Yes) {
		return;
	}

	DatFile &dat = tab->datFile();
	SpriteFile &spr = tab->spriteFile();

	// Build a set of all sprite IDs that are actively used by any thing
	std::unordered_set<uint32_t> usedSpriteIds;

	for (int cat = 0; cat < THING_CATEGORY_COUNT; ++cat) {
		ThingCategory category = static_cast<ThingCategory>(cat);
		const auto &things = dat.getThingTypes(category);
		uint16_t firstId = dat.getFirstId(category);

		for (size_t i = firstId; i < things.size(); ++i) {
			const ThingType *thing = dat.getThingType(static_cast<uint16_t>(i), category);
			if (!thing) {
				continue;
			}

			for (int fg = 0; fg < FRAME_GROUP_COUNT; ++fg) {
				const auto &frameGroup = thing->frameGroups[fg];
				if (!frameGroup) {
					continue;
				}

				for (uint32_t spriteId: frameGroup->spriteIds) {
					if (spriteId != 0) {
						usedSpriteIds.insert(spriteId);
					}
				}
			}
		}
	}

	// Iterate over all sprites and clear any that are not in the used set
	int removedCount = 0;
	uint32_t totalSprites = spr.spriteCount();

	for (uint32_t id = 1; id <= totalSprites; ++id) {
		if (usedSpriteIds.find(id) == usedSpriteIds.end()) {
			if (spr.clearSprite(id)) {
				++removedCount;
			}
		}
	}

	if (removedCount > 0) {
		tab->markDirty();
	}

	QMessageBox::information(this, "Remove Unused Sprites", QString("Removed %1 unused sprite(s).").arg(removedCount));

	Log().info("MainWindow: removed {} unused sprites", removedCount);
}
