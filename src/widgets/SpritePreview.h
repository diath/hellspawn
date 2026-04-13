/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <QImage>
#include <QPixmap>
#include <QWidget>

class QPainter;
class QMouseEvent;

class SpritePreview: public QWidget
{
		Q_OBJECT

	public:
		explicit SpritePreview(QWidget *parent = nullptr);
		~SpritePreview() override = default;

		void setSpriteImage(const QImage &image, uint32_t spriteId = 0);
		void clearImage();

		void setScale(int scale);
		int scale() const;

		void setShowCheckerboard(bool show);
		bool showCheckerboard() const;

		uint32_t spriteId() const;
		const QImage &spriteImage() const;
		bool hasImage() const;

		QSize sizeHint() const override;
		QSize minimumSizeHint() const override;

	signals:
		void clicked(uint32_t spriteId);
		void doubleClicked(uint32_t spriteId);

	protected:
		void paintEvent(QPaintEvent *event) override;
		void mousePressEvent(QMouseEvent *event) override;
		void mouseDoubleClickEvent(QMouseEvent *event) override;

	private:
		void drawCheckerboard(QPainter &painter, const QRect &area);

		QImage m_image;
		QPixmap m_cachedPixmap;
		int m_scale = 1;
		bool m_showCheckerboard = true;
		bool m_pixmapDirty = true;
		uint32_t m_spriteId = 0;
};
