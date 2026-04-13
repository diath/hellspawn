/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/LoadWidget.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

LoadWidget::LoadWidget(QWidget *parent)
: QWidget(parent)
{
	for (int i = 0; i < FieldCount; ++i) {
		m_combos[i] = nullptr;
		m_browseButtons[i] = nullptr;
	}
	m_loadButton = nullptr;
	m_newButton = nullptr;

	setupUi();
	loadRecentPaths();
}

LoadWidget::~LoadWidget()
{
	saveRecentPaths();
}

QString LoadWidget::sprPath() const { return m_combos[FieldSpr]->currentText().trimmed(); }
QString LoadWidget::datPath() const { return m_combos[FieldDat]->currentText().trimmed(); }
QString LoadWidget::otbPath() const { return m_combos[FieldOtb]->currentText().trimmed(); }
QString LoadWidget::xmlPath() const { return m_combos[FieldXml]->currentText().trimmed(); }

QString LoadWidget::pathForField(Field field) const
{
	if (field >= 0 && field < FieldCount) {
		return m_combos[field]->currentText().trimmed();
	}
	return {};
}

void LoadWidget::setPathForField(Field field, const QString &path)
{
	if (field >= 0 && field < FieldCount) {
		m_combos[field]->setCurrentText(path);
	}
}

void LoadWidget::addRecentPath(Field field, const QString &path)
{
	if (field < 0 || field >= FieldCount || path.isEmpty()) {
		return;
	}

	QComboBox *combo = m_combos[field];

	// Check if the path is already in the list
	int existingIndex = combo->findText(path);
	if (existingIndex >= 0) {
		// Move it to the top
		combo->removeItem(existingIndex);
	}

	// Insert at the top
	combo->insertItem(0, path);
	combo->setCurrentIndex(0);

	// Limit to 10 recent paths
	while (combo->count() > 10) {
		combo->removeItem(combo->count() - 1);
	}
}

void LoadWidget::setupUi()
{
	auto *outerLayout = new QVBoxLayout(this);
	outerLayout->setContentsMargins(40, 40, 40, 40);
	outerLayout->setSpacing(20);

	// Center the group box vertically
	outerLayout->addStretch(1);

	// Main group box for file path configuration
	auto *groupBox = new QGroupBox("File Configuration");
	auto *groupLayout = new QVBoxLayout(groupBox);
	groupLayout->setContentsMargins(16, 20, 16, 16);
	groupLayout->setSpacing(12);

	// Description label
	auto *descLabel = new QLabel(
	    "Select the game data files to edit. The .spr, .dat, and .otb files are required.\n"
	    "The .xml file is optional and provides item names for display."
	);
	descLabel->setWordWrap(true);
	groupLayout->addWidget(descLabel);

	// File path fields
	struct FieldDef
	{
			const char *label;
			const char *filter;
			const char *dialogTitle;
			bool required;
	};

	static const FieldDef fieldDefs[FieldCount] = {
	    {.label = "Sprite File (.spr):", .filter = "Sprite Files (*.spr);;All Files (*)", .dialogTitle = "Select SPR File", .required = true},
	    {.label = "Data File (.dat):", .filter = "Data Files (*.dat);;All Files (*)", .dialogTitle = "Select DAT File", .required = true},
	    {.label = "Items File (.otb):", .filter = "OTB Files (*.otb);;All Files (*)", .dialogTitle = "Select OTB File", .required = true},
	    {.label = "Items XML (.xml):", .filter = "XML Files (*.xml);;All Files (*)", .dialogTitle = "Select XML File (Optional)", .required = false},
	};

	for (int i = 0; i < FieldCount; ++i) {
		const auto &def = fieldDefs[i];

		auto *fieldLayout = new QHBoxLayout;
		fieldLayout->setSpacing(8);

		auto *label = new QLabel(def.label);
		label->setFixedWidth(140);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		if (def.required) {
			label->setText(QString("<b>%1</b>").arg(def.label));
		}

		auto *combo = new QComboBox;
		combo->setEditable(true);
		combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		combo->setMinimumWidth(300);
		combo->setMaxCount(10);
		combo->setInsertPolicy(QComboBox::NoInsert);
		m_combos[i] = combo;

		auto *browseButton = new QPushButton("Browse...");
		browseButton->setFixedWidth(80);

		const int fieldIndex = i;
		const QString filter = QString::fromUtf8(def.filter);
		const QString title = QString::fromUtf8(def.dialogTitle);

		connect(browseButton, &QPushButton::clicked, this, [this, fieldIndex, filter, title]() {
			onBrowseClicked(static_cast<Field>(fieldIndex), title, filter);
		});

		m_browseButtons[i] = browseButton;

		fieldLayout->addWidget(label);
		fieldLayout->addWidget(combo, 1);
		fieldLayout->addWidget(browseButton);

		groupLayout->addLayout(fieldLayout);
	}

	// Spacer before buttons
	groupLayout->addSpacing(8);

	// Load button
	auto *loadButtonLayout = new QHBoxLayout;
	loadButtonLayout->setSpacing(8);
	loadButtonLayout->addStretch();

	m_loadButton = new QPushButton("Load Files");
	m_loadButton->setFixedWidth(140);
	m_loadButton->setFixedHeight(32);
	m_loadButton->setDefault(true);
	m_loadButton->setStyleSheet(
	    "QPushButton {"
	    "  background-color: #1976d2;"
	    "  color: white;"
	    "  border: 1px solid #1565c0;"
	    "  border-radius: 4px;"
	    "  font-weight: bold;"
	    "  padding: 4px 16px;"
	    "}"
	    "QPushButton:hover {"
	    "  background-color: #1e88e5;"
	    "}"
	    "QPushButton:pressed {"
	    "  background-color: #1565c0;"
	    "}"
	);
	connect(m_loadButton, &QPushButton::clicked, this, &LoadWidget::loadRequested);
	loadButtonLayout->addWidget(m_loadButton);
	loadButtonLayout->addStretch();

	groupLayout->addLayout(loadButtonLayout);

	// Horizontal separator
	auto *separator = new QFrame;
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	groupLayout->addWidget(separator);

	// New button
	auto *newButtonLayout = new QHBoxLayout;
	newButtonLayout->setSpacing(8);
	newButtonLayout->addStretch();

	m_newButton = new QPushButton("New (Empty Editor)");
	m_newButton->setFixedWidth(160);
	m_newButton->setFixedHeight(28);
	m_newButton->setToolTip("Create a new blank editor state without loading any files.");
	connect(m_newButton, &QPushButton::clicked, this, &LoadWidget::newRequested);
	newButtonLayout->addWidget(m_newButton);
	newButtonLayout->addStretch();

	groupLayout->addLayout(newButtonLayout);

	outerLayout->addWidget(groupBox);
	outerLayout->addStretch(1);
}

void LoadWidget::onBrowseClicked(Field field, const QString &title, const QString &filter)
{
	QString startDir;
	QString currentPath = m_combos[field]->currentText().trimmed();
	if (!currentPath.isEmpty()) {
		QFileInfo info(currentPath);
		startDir = info.absolutePath();
	}
	if (startDir.isEmpty()) {
		startDir = QDir::currentPath();
	}

	QString path = QFileDialog::getOpenFileName(this, title, startDir, filter);
	if (!path.isEmpty()) {
		addRecentPath(field, path);
	}
}

void LoadWidget::loadRecentPaths()
{
	QSettings settings("Hellspawn", "Editor");

	static const char *keys[FieldCount] = {
	    "RecentPaths/spr",
	    "RecentPaths/dat",
	    "RecentPaths/otb",
	    "RecentPaths/xml",
	};

	for (int i = 0; i < FieldCount; ++i) {
		QStringList paths = settings.value(keys[i]).toStringList();
		for (const QString &path: paths) {
			if (!path.isEmpty()) {
				m_combos[i]->addItem(path);
			}
		}
	}
}

void LoadWidget::saveRecentPaths()
{
	QSettings settings("Hellspawn", "Editor");

	static const char *keys[FieldCount] = {
	    "RecentPaths/spr",
	    "RecentPaths/dat",
	    "RecentPaths/otb",
	    "RecentPaths/xml",
	};

	for (int i = 0; i < FieldCount; ++i) {
		QStringList paths;
		for (int j = 0; j < m_combos[i]->count() && j < 10; ++j) {
			QString path = m_combos[i]->itemText(j).trimmed();
			if (!path.isEmpty()) {
				paths << path;
			}
		}
		settings.setValue(keys[i], paths);
	}
}
