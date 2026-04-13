/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "editor/ThingIconProvider.h"

#include "core/SpriteFile.h"
#include "util/Logger.h"

#include <QPainter>
#include <QPixmap>

#include <algorithm>

// ---------------------------------------------------------------------------
// IconWorker
// ---------------------------------------------------------------------------

IconWorker::IconWorker(const SpriteFile *spriteFile, int iconSize, QObject *parent)
: QObject(parent)
, m_spriteFile(spriteFile)
, m_iconSize(iconSize)
{
	connect(this, &IconWorker::wakeUp, this, &IconWorker::processBatch, Qt::QueuedConnection);
}

void IconWorker::enqueue(IconBuildRequest request)
{
	{
		QMutexLocker lock(&m_mutex);
		m_queue.push_back(std::move(request));
	}
	emit wakeUp();
}

void IconWorker::enqueueBatch(std::vector<IconBuildRequest> requests)
{
	{
		QMutexLocker lock(&m_mutex);
		for (auto &r: requests) {
			m_queue.push_back(std::move(r));
		}
	}
	emit wakeUp();
}

void IconWorker::cancelAll()
{
	QMutexLocker lock(&m_mutex);
	m_queue.clear();
}

void IconWorker::cancelCategory(ThingCategory category)
{
	QMutexLocker lock(&m_mutex);
	std::erase_if(m_queue, [category](const IconBuildRequest &r) {
		return r.category == category;
	});
}

void IconWorker::processBatch()
{
	// Process a batch of requests per event-loop iteration.  Yielding back to
	// the event loop between batches lets cancelAll() / cancelCategory() take
	// effect promptly.
	constexpr int BATCH_SIZE = 64;

	for (int i = 0; i < BATCH_SIZE; ++i) {
		IconBuildRequest request;
		{
			QMutexLocker lock(&m_mutex);
			if (m_queue.empty())
				return;
			request = std::move(m_queue.front());
			m_queue.pop_front();
		}

		QImage image = buildIcon(request);
		emit imageReady(request.category, request.thingId, std::move(image));
	}

	// If there are more items remaining, schedule another batch.
	{
		QMutexLocker lock(&m_mutex);
		if (!m_queue.empty()) {
			emit wakeUp();
		}
	}
}

QImage IconWorker::buildIcon(const IconBuildRequest &request) const
{
	if (!m_spriteFile || request.spriteIds.empty()) {
		return {};
	}

	int w = request.width;
	int h = request.height;

	// Compute base sprite index offset for direction selection.
	// For creatures, use xPattern=2 (south-facing) if enough patterns exist.
	int baseSpriteOffset = 0;
	if (request.isCreature && request.patternX >= 3) {
		baseSpriteOffset = 2 * w * h * static_cast<int>(request.layers);
	}

	// Compose the native-resolution image from sprite tiles
	int nativeW = w * SPRITE_SIZE;
	int nativeH = h * SPRITE_SIZE;

	QImage composite(nativeW, nativeH, QImage::Format_RGBA8888);
	composite.fill(Qt::transparent);

	bool anyPixel = false;
	{
		QPainter painter(&composite);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

		for (int ty = 0; ty < h; ++ty) {
			for (int tx = 0; tx < w; ++tx) {
				int index = baseSpriteOffset + ty * w + tx;
				if (index >= static_cast<int>(request.spriteIds.size()))
					continue;

				uint32_t spriteId = request.spriteIds[index];
				if (spriteId == 0)
					continue;

				// Thread-safe: reads only immutable file data.
				QImage spriteImg = m_spriteFile->getSpriteImageDirect(spriteId);
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

	// Build the final ICON_SIZE x ICON_SIZE canvas
	QImage canvas(m_iconSize, m_iconSize, QImage::Format_RGBA8888);
	canvas.fill(Qt::transparent);

	if (w > 2 || h > 2) {
		// Large multi-tile: downscale to fit within iconSize x iconSize
		QImage scaled = composite.scaled(m_iconSize, m_iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		int xOff = (m_iconSize - scaled.width()) / 2;
		int yOff = (m_iconSize - scaled.height()) / 2;

		QPainter p(&canvas);
		p.drawImage(xOff, yOff, scaled);
		p.end();
	} else {
		// Small (w<=2 && h<=2): center at native resolution
		int xOff = (m_iconSize - nativeW) / 2;
		int yOff = (m_iconSize - nativeH) / 2;

		QPainter p(&canvas);
		p.drawImage(xOff, yOff, composite);
		p.end();
	}

	return canvas;
}

// ---------------------------------------------------------------------------
// ThingIconProvider
// ---------------------------------------------------------------------------

ThingIconProvider::ThingIconProvider(QObject *parent)
: QObject(parent)
{
}

ThingIconProvider::~ThingIconProvider()
{
	stopWorkerThread();
}

void ThingIconProvider::setSpriteFile(const SpriteFile *spriteFile)
{
	if (m_spriteFile == spriteFile)
		return;

	// Changing the file invalidates everything.
	cancelAll();
	invalidateAll();
	stopWorkerThread();

	m_spriteFile = spriteFile;
}

// ---------------------------------------------------------------------------
// Cache accessors
// ---------------------------------------------------------------------------

QIcon ThingIconProvider::getCachedIcon(ThingCategory category, uint16_t thingId) const
{
	CacheKey key {category, thingId};
	auto it = m_cache.find(key);
	if (it != m_cache.end()) {
		return it->second.icon;
	}
	return {};
}

size_t ThingIconProvider::cacheSize() const
{
	return m_cache.size();
}

// ---------------------------------------------------------------------------
// Request / enqueue
// ---------------------------------------------------------------------------

QIcon ThingIconProvider::requestIcon(ThingCategory category, uint16_t thingId, const ThingType *thing)
{
	CacheKey key {category, thingId};

	// Return cached icon immediately if available.
	auto cacheIt = m_cache.find(key);
	if (cacheIt != m_cache.end()) {
		touchCache(key);
		return cacheIt->second.icon;
	}

	// Already pending — don't enqueue again.
	if (m_pending.contains(key)) {
		return {};
	}

	if (!thing || !m_spriteFile) {
		return {};
	}

	IconBuildRequest request = makeBuildRequest(category, thingId, thing);
	if (request.spriteIds.empty()) {
		return {};
	}

	m_pending.insert(key);

	ensureWorkerThread();
	m_worker->enqueue(std::move(request));

	return {};
}

IconBuildRequest ThingIconProvider::makeBuildRequest(ThingCategory category, uint16_t thingId, const ThingType *thing)
{
	IconBuildRequest req;
	req.category = category;
	req.thingId = thingId;
	req.isCreature = (category == ThingCategory::Creature);

	if (!thing)
		return req;

	const FrameGroup *fg = thing->getFrameGroup();
	if (!fg || fg->spriteIds.empty())
		return req;

	req.width = fg->width;
	req.height = fg->height;
	req.layers = fg->layers;
	req.patternX = fg->patternX;
	req.spriteIds = fg->spriteIds; // deep copy — snapshot at request time

	return req;
}

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

void ThingIconProvider::cancelAll()
{
	m_pending.clear();
	if (m_worker) {
		m_worker->cancelAll();
	}
}

void ThingIconProvider::cancelCategory(ThingCategory category)
{
	std::erase_if(m_pending, [category](const CacheKey &k) {
		return k.category == category;
	});
	if (m_worker) {
		m_worker->cancelCategory(category);
	}
}

// ---------------------------------------------------------------------------
// Cache invalidation
// ---------------------------------------------------------------------------

void ThingIconProvider::invalidate(ThingCategory category, uint16_t thingId)
{
	CacheKey key {category, thingId};
	auto it = m_cache.find(key);
	if (it != m_cache.end()) {
		m_lruList.erase(it->second.lruIter);
		m_cache.erase(it);
	}
	m_pending.erase(key);
}

void ThingIconProvider::invalidateCategory(ThingCategory category)
{
	for (auto it = m_cache.begin(); it != m_cache.end();) {
		if (it->first.category == category) {
			m_lruList.erase(it->second.lruIter);
			it = m_cache.erase(it);
		} else {
			++it;
		}
	}
	std::erase_if(m_pending, [category](const CacheKey &k) {
		return k.category == category;
	});
}

void ThingIconProvider::invalidateAll()
{
	m_cache.clear();
	m_lruList.clear();
	m_pending.clear();
}

// ---------------------------------------------------------------------------
// Direct cache insertion (for synchronous single-item rebuilds)
// ---------------------------------------------------------------------------

void ThingIconProvider::insertIcon(ThingCategory category, uint16_t thingId, const QIcon &icon)
{
	CacheKey key {category, thingId};

	auto existing = m_cache.find(key);
	if (existing != m_cache.end()) {
		// Update in place and touch.
		existing->second.icon = icon;
		touchCache(key);
		return;
	}

	evictIfNeeded();
	m_lruList.push_front(key);
	m_cache[key] = CacheEntry {icon, m_lruList.begin()};
}

// ---------------------------------------------------------------------------
// Image delivery from worker thread (queued connection → main thread)
// ---------------------------------------------------------------------------

void ThingIconProvider::onImageReady(ThingCategory category, uint16_t thingId, QImage image)
{
	CacheKey key {category, thingId};

	// If the request was cancelled while in-flight, discard the result.
	if (!m_pending.contains(key)) {
		return;
	}
	m_pending.erase(key);

	// Convert QImage → QPixmap → QIcon on the GUI thread (QPixmap is not
	// thread-safe in older Qt builds).
	QIcon icon;
	if (!image.isNull()) {
		icon = QIcon(QPixmap::fromImage(std::move(image)));
	}

	if (icon.isNull()) {
		// Building failed — nothing to cache or deliver.
		return;
	}

	// Insert into the LRU cache.
	evictIfNeeded();
	m_lruList.push_front(key);
	m_cache[key] = CacheEntry {icon, m_lruList.begin()};

	emit iconReady(category, thingId, icon);
}

// ---------------------------------------------------------------------------
// LRU helpers
// ---------------------------------------------------------------------------

void ThingIconProvider::touchCache(const CacheKey &key)
{
	auto it = m_cache.find(key);
	if (it == m_cache.end())
		return;

	m_lruList.erase(it->second.lruIter);
	m_lruList.push_front(key);
	it->second.lruIter = m_lruList.begin();
}

void ThingIconProvider::evictIfNeeded()
{
	while (m_cache.size() >= m_cacheLimit && !m_lruList.empty()) {
		const CacheKey &victim = m_lruList.back();
		m_cache.erase(victim);
		m_lruList.pop_back();
	}
}

// ---------------------------------------------------------------------------
// Placeholder icon
// ---------------------------------------------------------------------------

const QIcon &ThingIconProvider::placeholderIcon()
{
	if (!m_placeholderBuilt) {
		buildPlaceholderIcon();
		m_placeholderBuilt = true;
	}
	return m_placeholderIcon;
}

void ThingIconProvider::buildPlaceholderIcon()
{
	QImage img(ICON_SIZE, ICON_SIZE, QImage::Format_ARGB32);
	img.fill(Qt::transparent);

	QPainter painter(&img);
	constexpr int checkerSize = 32;
	const int cellSize = 8;
	const int offset = (ICON_SIZE - checkerSize) / 2;
	QColor lightColor(70, 70, 70);
	QColor darkColor(55, 55, 55);

	for (int y = 0; y < checkerSize; y += cellSize) {
		for (int x = 0; x < checkerSize; x += cellSize) {
			int col = x / cellSize;
			int row = y / cellSize;
			bool light = ((col + row) % 2 == 0);
			painter.fillRect(offset + x, offset + y, cellSize, cellSize, light ? lightColor : darkColor);
		}
	}
	painter.end();

	m_placeholderIcon = QIcon(QPixmap::fromImage(img));
}

// ---------------------------------------------------------------------------
// Worker thread lifecycle
// ---------------------------------------------------------------------------

void ThingIconProvider::ensureWorkerThread()
{
	if (m_workerThread)
		return;

	m_workerThread = new QThread(this);
	m_workerThread->setObjectName(QStringLiteral("IconWorker"));

	m_worker = new IconWorker(m_spriteFile, ICON_SIZE);
	m_worker->moveToThread(m_workerThread);

	// Worker → Provider (queued, crosses thread boundary)
	connect(m_worker, &IconWorker::imageReady, this, &ThingIconProvider::onImageReady, Qt::QueuedConnection);

	// Clean up the worker when the thread finishes.
	connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

	m_workerThread->start();

	Log().debug("ThingIconProvider: worker thread started");
}

void ThingIconProvider::stopWorkerThread()
{
	if (!m_workerThread)
		return;

	if (m_worker) {
		m_worker->cancelAll();
	}

	m_workerThread->quit();
	m_workerThread->wait();

	// Worker is deleted by the finished → deleteLater connection.
	m_worker = nullptr;
	delete m_workerThread;
	m_workerThread = nullptr;

	Log().debug("ThingIconProvider: worker thread stopped");
}