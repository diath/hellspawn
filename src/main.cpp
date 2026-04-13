/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/MainWindow.h"
#include "util/Logger.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	app.setApplicationName("Hellspawn Editor");
	app.setApplicationVersion("1.0.0");
	app.setOrganizationName("Hellspawn");
	app.setOrganizationDomain("hellspawn.editor");
	app.setWindowIcon(QIcon(":/hellspawn.png"));

	Log().setMinLevel(LogLevel::Debug);
	Log().info("Hellspawn Editor starting...");

	MainWindow mainWindow;
	mainWindow.show();

	int result = app.exec();

	Log().info("Hellspawn Editor shutting down (exit code {})", result);
	return result;
}
