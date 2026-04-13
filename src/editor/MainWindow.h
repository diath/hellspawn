/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabWidget>

class EditorTab;

class MainWindow: public QMainWindow
{
		Q_OBJECT

	public:
		explicit MainWindow(QWidget *parent = nullptr);
		~MainWindow() override = default;

		EditorTab *currentEditorTab() const;
		EditorTab *editorTabAt(int index) const;
		int editorTabCount() const;

	public slots:
		void newTab();
		void closeTab(int index);
		void closeCurrentTab();
		void saveCurrentTab();
		void saveCurrentTabAs();
		void onTabChanged(int index);
		void onTabDirtyChanged(int tabIndex, bool dirty);
		void updateTabTitle(int tabIndex);
		void updateWindowTitle();
		void showAbout();
		void clearMarketData();
		void removeUnusedSprites();

	protected:
		void closeEvent(QCloseEvent *event) override;

	private:
		void setupMenuBar();
		void setupTabWidget();
		bool confirmCloseTab(int index);
		bool confirmCloseAll();

		QTabWidget *m_tabWidget = nullptr;

		// Menus
		QMenu *m_fileMenu = nullptr;
		QMenu *m_editMenu = nullptr;
		QMenu *m_actionsMenu = nullptr;
		QMenu *m_helpMenu = nullptr;

		// File actions
		QAction *m_newTabAction = nullptr;
		QAction *m_closeTabAction = nullptr;
		QAction *m_saveAction = nullptr;
		QAction *m_saveAsAction = nullptr;
		QAction *m_exitAction = nullptr;

		// Actions menu actions
		QAction *m_clearMarketDataAction = nullptr;
		QAction *m_removeUnusedSpritesAction = nullptr;
		QAction *m_clearClipboardThingAction = nullptr;

		// Help actions
		QAction *m_aboutAction = nullptr;
};
