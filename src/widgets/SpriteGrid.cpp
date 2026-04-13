/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "widgets/SpriteGrid.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>

// =============================================================================
// SpriteCell implementation
// =============================================================================

SpriteCell::SpriteCell(uint32_t spriteId, QWidget *parent)
: QWidget(parent)
, m_spriteId(spriteId)
{
	setFixedSize(CELL_SIZE, CELL_SIZE);
	setMouseTracking(true);
	setCursor(Qt::PointingHandCursor);
	setToolTip(QString("Sprite #%1").arg(spriteId));
}

void SpriteCell::setSpriteImage(const QImage &image)
{
	m_image = image;
	m_pixmapDirty = true;
	update();
}

void SpriteCell::setSelected(bool selected)
{
	if (m_selected != selected) {
		m_selected = selected;
		update();
	}
}

bool SpriteCell::isSelected() const { return m_selected; }
uint32_t SpriteCell::spriteId() const { return m_spriteId; }
const QImage &SpriteCell::spriteImage() const { return m_image; }

void SpriteCell::paintEvent(QPaintEvent * /*event*/)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	// Background
	if (m_selected) {
		painter.fillRect(rect(), QColor(60, 120, 215));
	} else if (m_hovered) {
		painter.fillRect(rect(), QColor(60, 120, 215, 60));
	} else {
		painter.fillRect(rect(), palette().window().color());
	}

	// Checkerboard behind sprite
	QRect spriteRect(PADDING, PADDING, SPRITE_DRAW_SIZE, SPRITE_DRAW_SIZE);
	drawCheckerboard(painter, spriteRect);

	// Draw sprite
	if (!m_image.isNull()) {
		if (m_pixmapDirty) {
			m_cachedPixmap = QPixmap::fromImage(m_image.scaled(
			    SPRITE_DRAW_SIZE, SPRITE_DRAW_SIZE, Qt::KeepAspectRatio, Qt::FastTransformation
			));
			m_pixmapDirty = false;
		}
		int xOff = PADDING + (SPRITE_DRAW_SIZE - m_cachedPixmap.width()) / 2;
		int yOff = PADDING + (SPRITE_DRAW_SIZE - m_cachedPixmap.height()) / 2;
		painter.drawPixmap(xOff, yOff, m_cachedPixmap);
	}

	// Border
	if (m_selected) {
		painter.setPen(QPen(QColor(40, 90, 180), 2));
		painter.drawRect(1, 1, CELL_SIZE - 2, CELL_SIZE - 2);
	} else {
		painter.setPen(QPen(QColor(100, 100, 100), 1));
		painter.drawRect(0, 0, CELL_SIZE - 1, CELL_SIZE - 1);
	}
}

void SpriteCell::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
		emit clicked(m_spriteId, event->button(), event->modifiers());
	}
	QWidget::mousePressEvent(event);
}

void SpriteCell::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit doubleClicked(m_spriteId);
	}
	QWidget::mouseDoubleClickEvent(event);
}

void SpriteCell::contextMenuEvent(QContextMenuEvent *event)
{
	emit contextMenuRequested(m_spriteId, event->globalPos());
	event->accept();
}

void SpriteCell::enterEvent(QEnterEvent * /*event*/)
{
	m_hovered = true;
	update();
}

void SpriteCell::leaveEvent(QEvent * /*event*/)
{
	m_hovered = false;
	update();
}

void SpriteCell::drawCheckerboard(QPainter &painter, const QRect &area)
{
	constexpr int cellSize = 4;
	QColor lightColor(70, 70, 70);
	QColor darkColor(55, 55, 55);

	for (int y = area.top(); y < area.bottom(); y += cellSize) {
		for (int x = area.left(); x < area.right(); x += cellSize) {
			int col = (x - area.left()) / cellSize;
			int row = (y - area.top()) / cellSize;
			bool light = ((col + row) % 2 == 0);
			QRect cellRect(x, y, qMin(cellSize, area.right() - x), qMin(cellSize, area.bottom() - y));
			painter.fillRect(cellRect, light ? lightColor : darkColor);
		}
	}
}

// =============================================================================
// SpriteGrid implementation
// =============================================================================

SpriteGrid::SpriteGrid(QWidget *parent)
: QWidget(parent)
{
	setupUi();
}

void SpriteGrid::setSpriteFile(const SpriteFile *spriteFile)
{
	m_spriteFile = spriteFile;
	m_selectedIds.clear();
	recalcPages();
	goToPage(0);
}

void SpriteGrid::setGridSize(int columns, int rows)
{
	if (columns < 1)
		columns = 1;
	if (rows < 1)
		rows = 1;
	m_columns = columns;
	m_rows = rows;
	recalcPages();
	goToPage(m_currentPage);
}

int SpriteGrid::currentPage() const { return m_currentPage; }
int SpriteGrid::totalPages() const { return m_totalPages; }
int SpriteGrid::spritesPerPage() const { return m_columns * m_rows; }

QSet<uint32_t> SpriteGrid::selectedSpriteIds() const { return m_selectedIds; }

uint32_t SpriteGrid::singleSelectedId() const
{
	if (m_selectedIds.size() == 1) {
		return *m_selectedIds.begin();
	}
	return 0;
}

void SpriteGrid::clearSelection()
{
	m_selectedIds.clear();
	updateCellSelections();
	emit selectionChanged(m_selectedIds);
}

void SpriteGrid::refresh()
{
	loadPageSprites();
}

void SpriteGrid::goToPage(int page)
{
	if (page < 0)
		page = 0;
	if (page >= m_totalPages && m_totalPages > 0)
		page = m_totalPages - 1;
	if (m_totalPages == 0)
		page = 0;

	m_currentPage = page;
	m_pageSpinBox->blockSignals(true);
	m_pageSpinBox->setValue(page + 1);
	m_pageSpinBox->blockSignals(false);

	m_pageLabel->setText(QString("/ %1").arg(m_totalPages));

	m_prevButton->setEnabled(m_currentPage > 0);
	m_nextButton->setEnabled(m_currentPage < m_totalPages - 1);
	m_firstButton->setEnabled(m_currentPage > 0);
	m_lastButton->setEnabled(m_currentPage < m_totalPages - 1);

	loadPageSprites();
}

void SpriteGrid::goToFirstPage() { goToPage(0); }
void SpriteGrid::goToLastPage() { goToPage(m_totalPages - 1); }
void SpriteGrid::goToPrevPage() { goToPage(m_currentPage - 1); }
void SpriteGrid::goToNextPage() { goToPage(m_currentPage + 1); }

void SpriteGrid::goToSpriteId(uint32_t spriteId)
{
	if (!m_spriteFile || spriteId == 0)
		return;

	int perPage = spritesPerPage();
	if (perPage <= 0)
		return;

	int index = static_cast<int>(spriteId - 1);
	int page = index / perPage;
	goToPage(page);

	m_selectedIds.clear();
	m_selectedIds.insert(spriteId);
	updateCellSelections();
	emit selectionChanged(m_selectedIds);
}

void SpriteGrid::setupUi()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(4);

	// Grid area inside a scroll area
	m_gridWidget = new QWidget;
	m_gridLayout = new QGridLayout(m_gridWidget);
	m_gridLayout->setContentsMargins(2, 2, 2, 2);
	m_gridLayout->setSpacing(2);

	m_scrollArea = new QScrollArea;
	m_scrollArea->setWidget(m_gridWidget);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	mainLayout->addWidget(m_scrollArea, 1);

	// Navigation bar
	auto *navLayout = new QHBoxLayout;
	navLayout->setContentsMargins(4, 2, 4, 2);
	navLayout->setSpacing(4);

	m_firstButton = new QToolButton;
	m_firstButton->setText(QStringLiteral("<<"));
	m_firstButton->setToolTip("First Page");
	connect(m_firstButton, &QToolButton::clicked, this, &SpriteGrid::goToFirstPage);

	m_prevButton = new QToolButton;
	m_prevButton->setText(QStringLiteral("<"));
	m_prevButton->setToolTip("Previous Page");
	connect(m_prevButton, &QToolButton::clicked, this, &SpriteGrid::goToPrevPage);

	m_pageSpinBox = new QSpinBox;
	m_pageSpinBox->setMinimum(1);
	m_pageSpinBox->setMaximum(1);
	m_pageSpinBox->setFixedWidth(80);
	m_pageSpinBox->setAlignment(Qt::AlignCenter);
	connect(m_pageSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) { goToPage(value - 1); });

	m_pageLabel = new QLabel("/ 1");

	m_nextButton = new QToolButton;
	m_nextButton->setText(QStringLiteral(">"));
	m_nextButton->setToolTip("Next Page");
	connect(m_nextButton, &QToolButton::clicked, this, &SpriteGrid::goToNextPage);

	m_lastButton = new QToolButton;
	m_lastButton->setText(QStringLiteral(">>"));
	m_lastButton->setToolTip("Last Page");
	connect(m_lastButton, &QToolButton::clicked, this, &SpriteGrid::goToLastPage);

	m_spriteCountLabel = new QLabel("0 sprites");

	navLayout->addWidget(m_firstButton);
	navLayout->addWidget(m_prevButton);
	navLayout->addWidget(m_pageSpinBox);
	navLayout->addWidget(m_pageLabel);
	navLayout->addWidget(m_nextButton);
	navLayout->addWidget(m_lastButton);
	navLayout->addStretch();
	navLayout->addWidget(m_spriteCountLabel);

	mainLayout->addLayout(navLayout);
}

void SpriteGrid::recalcPages()
{
	int perPage = spritesPerPage();
	uint32_t count = m_spriteFile ? m_spriteFile->spriteCount() : 0;

	if (count == 0 || perPage <= 0) {
		m_totalPages = 0;
	} else {
		m_totalPages = static_cast<int>((count + perPage - 1) / perPage);
	}

	m_pageSpinBox->setMaximum(qMax(1, m_totalPages));

	m_spriteCountLabel->setText(
	    QString("%1 sprites").arg(count)
	);

	if (m_currentPage >= m_totalPages && m_totalPages > 0) {
		m_currentPage = m_totalPages - 1;
	}
}

void SpriteGrid::loadPageSprites()
{
	// Clear existing cells
	for (auto *cell: m_cells) {
		m_gridLayout->removeWidget(cell);
		cell->deleteLater();
	}
	m_cells.clear();

	if (!m_spriteFile || m_totalPages == 0) {
		return;
	}

	int perPage = spritesPerPage();
	uint32_t startId = static_cast<uint32_t>(m_currentPage) * static_cast<uint32_t>(perPage) + 1;
	uint32_t endId = startId + static_cast<uint32_t>(perPage);
	uint32_t maxId = m_spriteFile->spriteCount();

	if (endId > maxId + 1) {
		endId = maxId + 1;
	}

	for (uint32_t id = startId; id < endId; ++id) {
		int index = static_cast<int>(id - startId);
		int row = index / m_columns;
		int col = index % m_columns;

		auto *cell = new SpriteCell(id, m_gridWidget);

		QImage img = m_spriteFile->getSpriteImage(id);
		if (!img.isNull()) {
			cell->setSpriteImage(img);
		}

		cell->setSelected(m_selectedIds.contains(id));

		connect(cell, &SpriteCell::clicked, this, &SpriteGrid::onCellClicked);
		connect(cell, &SpriteCell::doubleClicked, this, &SpriteGrid::onCellDoubleClicked);
		connect(cell, &SpriteCell::contextMenuRequested, this, &SpriteGrid::onCellContextMenu);

		m_gridLayout->addWidget(cell, row, col);
		m_cells.push_back(cell);
	}

	emit pageChanged(m_currentPage);
}

void SpriteGrid::updateCellSelections()
{
	for (auto *cell: m_cells) {
		cell->setSelected(m_selectedIds.contains(cell->spriteId()));
	}
}

void SpriteGrid::onCellClicked(uint32_t spriteId, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
	if (button == Qt::LeftButton) {
		if (modifiers & Qt::ControlModifier) {
			// Toggle selection
			if (m_selectedIds.contains(spriteId)) {
				m_selectedIds.remove(spriteId);
			} else {
				m_selectedIds.insert(spriteId);
			}
		} else if (modifiers & Qt::ShiftModifier) {
			// Range selection from last selected to this one
			if (!m_selectedIds.isEmpty()) {
				uint32_t lastId = 0;
				for (auto id: m_selectedIds) {
					if (id > lastId)
						lastId = id;
				}
				uint32_t minId = qMin(lastId, spriteId);
				uint32_t maxId = qMax(lastId, spriteId);
				for (uint32_t id = minId; id <= maxId; ++id) {
					m_selectedIds.insert(id);
				}
			} else {
				m_selectedIds.insert(spriteId);
			}
		} else {
			// Single selection
			m_selectedIds.clear();
			m_selectedIds.insert(spriteId);
		}
		updateCellSelections();
		emit selectionChanged(m_selectedIds);
	} else if (button == Qt::RightButton) {
		if (!m_selectedIds.contains(spriteId)) {
			m_selectedIds.clear();
			m_selectedIds.insert(spriteId);
			updateCellSelections();
			emit selectionChanged(m_selectedIds);
		}
	}
}

void SpriteGrid::onCellDoubleClicked(uint32_t spriteId)
{
	emit spriteDoubleClicked(spriteId);
}

void SpriteGrid::onCellContextMenu(uint32_t spriteId, const QPoint &globalPos)
{
	emit spriteContextMenu(spriteId, globalPos);
}
