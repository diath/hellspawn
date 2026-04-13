/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <QComboBox>
#include <QPushButton>
#include <QWidget>

class LoadWidget: public QWidget
{
		Q_OBJECT

	public:
		enum Field : int
		{
			FieldSpr = 0,
			FieldDat = 1,
			FieldOtb = 2,
			FieldXml = 3,
			FieldCount = 4
		};

		explicit LoadWidget(QWidget *parent = nullptr);
		~LoadWidget() override;

		QString sprPath() const;
		QString datPath() const;
		QString otbPath() const;
		QString xmlPath() const;

		QString pathForField(Field field) const;
		void setPathForField(Field field, const QString &path);
		void addRecentPath(Field field, const QString &path);

	signals:
		void loadRequested();
		void newRequested();

	private:
		void setupUi();
		void onBrowseClicked(Field field, const QString &title, const QString &filter);
		void loadRecentPaths();
		void saveRecentPaths();

		QComboBox *m_combos[FieldCount] = {};
		QPushButton *m_browseButtons[FieldCount] = {};
		QPushButton *m_loadButton = nullptr;
		QPushButton *m_newButton = nullptr;
};
