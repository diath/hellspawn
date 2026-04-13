/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/SpriteFile.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <vector>

class SpriteCell: public QWidget
{
		Q_OBJECT

	public:
		explicit SpriteCell(uint32_t spriteId, QWidget *parent = nullptr);
		~SpriteCell() override = default;

		void setSpriteImage(const QImage &image);
		void setSelected(bool selected);

		bool isSelected() const;
		uint32_t spriteId() const;
		const QImage &spriteImage() const;

		static constexpr int CELL_SIZE = 40;
		static constexpr int SPRITE_DRAW_SIZE = 32;
		static constexpr int PADDING = (CELL_SIZE - SPRITE_DRAW_SIZE) / 2;

	signals:
		void clicked(uint32_t spriteId, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
		void doubleClicked(uint32_t spriteId);
		void contextMenuRequested(uint32_t spriteId, const QPoint &globalPos);

	protected:
		void paintEvent(QPaintEvent *event) override;
		void mousePressEvent(QMouseEvent *event) override;
		void mouseDoubleClickEvent(QMouseEvent *event) override;
		void contextMenuEvent(QContextMenuEvent *event) override;
		void enterEvent(QEnterEvent *event) override;
		void leaveEvent(QEvent *event) override;

	private:
		void drawCheckerboard(QPainter &painter, const QRect &area);

		uint32_t m_spriteId;
		QImage m_image;
		QPixmap m_cachedPixmap;
		bool m_selected = false;
		bool m_hovered = false;
		bool m_pixmapDirty = true;
};

class SpriteGrid: public QWidget
{
		Q_OBJECT

	public:
		explicit SpriteGrid(QWidget *parent = nullptr);
		~SpriteGrid() override = default;

		void setSpriteFile(const SpriteFile *spriteFile);
		void setGridSize(int columns, int rows);

		int currentPage() const;
		int totalPages() const;
		int spritesPerPage() const;

		QSet<uint32_t> selectedSpriteIds() const;
		uint32_t singleSelectedId() const;

		void clearSelection();
		void refresh();

	public slots:
		void goToPage(int page);
		void goToFirstPage();
		void goToLastPage();
		void goToPrevPage();
		void goToNextPage();
		void goToSpriteId(uint32_t spriteId);

	signals:
		void selectionChanged(const QSet<uint32_t> &selectedIds);
		void spriteDoubleClicked(uint32_t spriteId);
		void spriteContextMenu(uint32_t spriteId, const QPoint &globalPos);
		void pageChanged(int page);

	private:
		void setupUi();
		void recalcPages();
		void loadPageSprites();
		void updateCellSelections();

		void onCellClicked(uint32_t spriteId, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
		void onCellDoubleClicked(uint32_t spriteId);
		void onCellContextMenu(uint32_t spriteId, const QPoint &globalPos);

		const SpriteFile *m_spriteFile = nullptr;
		int m_currentPage = 0;
		int m_totalPages = 0;
		int m_columns = 24;
		int m_rows = 12;

		QSet<uint32_t> m_selectedIds;
		std::vector<SpriteCell *> m_cells;

		// UI elements
		QScrollArea *m_scrollArea = nullptr;
		QWidget *m_gridWidget = nullptr;
		QGridLayout *m_gridLayout = nullptr;
		QToolButton *m_firstButton = nullptr;
		QToolButton *m_prevButton = nullptr;
		QSpinBox *m_pageSpinBox = nullptr;
		QLabel *m_pageLabel = nullptr;
		QToolButton *m_nextButton = nullptr;
		QToolButton *m_lastButton = nullptr;
		QLabel *m_spriteCountLabel = nullptr;
};
