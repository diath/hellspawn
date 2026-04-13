/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "widgets/SpritePreview.h"

#include <QMouseEvent>
#include <QPainter>

SpritePreview::SpritePreview(QWidget *parent)
: QWidget(parent)
{
	setMinimumSize(34, 34);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SpritePreview::setSpriteImage(const QImage &image, uint32_t spriteId)
{
	m_image = image;
	m_spriteId = spriteId;
	m_pixmapDirty = true;
	update();
}

void SpritePreview::clearImage()
{
	m_image = QImage();
	m_spriteId = 0;
	m_pixmapDirty = true;
	update();
}

void SpritePreview::setScale(int scale)
{
	if (scale < 1)
		scale = 1;
	if (scale > 8)
		scale = 8;
	m_scale = scale;
	m_pixmapDirty = true;
	updateGeometry();
	update();
}

int SpritePreview::scale() const { return m_scale; }

void SpritePreview::setShowCheckerboard(bool show)
{
	m_showCheckerboard = show;
	update();
}

bool SpritePreview::showCheckerboard() const { return m_showCheckerboard; }

uint32_t SpritePreview::spriteId() const { return m_spriteId; }
const QImage &SpritePreview::spriteImage() const { return m_image; }
bool SpritePreview::hasImage() const { return !m_image.isNull(); }

QSize SpritePreview::sizeHint() const
{
	if (m_image.isNull()) {
		return {32 * m_scale + 2, 32 * m_scale + 2};
	}
	return {m_image.width() * m_scale + 2, m_image.height() * m_scale + 2};
}

QSize SpritePreview::minimumSizeHint() const
{
	return sizeHint();
}

void SpritePreview::paintEvent(QPaintEvent * /*event*/)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

	QRect drawArea(1, 1, width() - 2, height() - 2);

	// Draw border
	painter.setPen(QPen(palette().mid().color(), 1));
	painter.drawRect(0, 0, width() - 1, height() - 1);

	if (m_showCheckerboard) {
		drawCheckerboard(painter, drawArea);
	} else {
		painter.fillRect(drawArea, Qt::black);
	}

	if (!m_image.isNull()) {
		if (m_pixmapDirty) {
			QSize scaledSize(m_image.width() * m_scale, m_image.height() * m_scale);
			m_cachedPixmap = QPixmap::fromImage(
			    m_image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::FastTransformation)
			);
			m_pixmapDirty = false;
		}

		int xOff = drawArea.x() + (drawArea.width() - m_cachedPixmap.width()) / 2;
		int yOff = drawArea.y() + (drawArea.height() - m_cachedPixmap.height()) / 2;
		painter.drawPixmap(xOff, yOff, m_cachedPixmap);
	}
}

void SpritePreview::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit clicked(m_spriteId);
	}
	QWidget::mousePressEvent(event);
}

void SpritePreview::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit doubleClicked(m_spriteId);
	}
	QWidget::mouseDoubleClickEvent(event);
}

void SpritePreview::drawCheckerboard(QPainter &painter, const QRect &area)
{
	const int cellSize = 4 * m_scale;
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
