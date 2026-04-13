/*
 * Copyright (c) 2026, Kamil Chojnowski <Y29udGFjdEBkaWF0aC5uZXQ=>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/SpriteData.h"
#include "core/ThingData.h"

#include <QIcon>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>

#include <cstdint>
#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SpriteFile;

// ---------------------------------------------------------------------------
// IconBuildRequest — self-contained snapshot of everything needed to compose
// a thing icon on a background thread.  Captured at request time so the
// worker never touches ThingType* or any other main-thread-mutable state.
// ---------------------------------------------------------------------------

struct IconBuildRequest
{
		ThingCategory category {};
		uint16_t thingId = 0;

		// Frame-group geometry
		uint8_t width = 1;
		uint8_t height = 1;
		uint8_t layers = 1;
		uint8_t patternX = 1;

		// Sprite IDs for the first frame group (or south-facing for creatures)
		std::vector<uint32_t> spriteIds;

		// Whether this thing is a creature (affects direction selection)
		bool isCreature = false;
};

// ---------------------------------------------------------------------------
// IconWorker — lives on a dedicated QThread.  Pulls requests from a shared
// queue, composes QImages using the thread-safe SpriteFile::getSpriteImageDirect(),
// and delivers results back to the main thread via a queued signal.
// ---------------------------------------------------------------------------

class IconWorker: public QObject
{
		Q_OBJECT

	public:
		explicit IconWorker(const SpriteFile *spriteFile, int iconSize, QObject *parent = nullptr);

		/// Enqueue a request.  Thread-safe — called from the main thread.
		void enqueue(IconBuildRequest request);

		/// Enqueue a batch of requests.  Thread-safe.
		void enqueueBatch(std::vector<IconBuildRequest> requests);

		/// Cancel all pending requests.  Thread-safe.
		void cancelAll();

		/// Cancel pending requests for a specific category.  Thread-safe.
		void cancelCategory(ThingCategory category);

	signals:
		/// Emitted (from the worker thread) for each completed icon.
		/// Connected to ThingIconProvider with Qt::QueuedConnection.
		void imageReady(ThingCategory category, uint16_t thingId, QImage image);

		/// Internal signal used to wake up the processing loop.
		void wakeUp();

	private slots:
		void processBatch();

	private:
		QImage buildIcon(const IconBuildRequest &request) const;

		const SpriteFile *m_spriteFile = nullptr;
		int m_iconSize = 64;

		QMutex m_mutex;
		std::deque<IconBuildRequest> m_queue;
};

// ---------------------------------------------------------------------------
// ThingIconProvider — main-thread façade that manages:
//   • An LRU icon cache with a configurable size limit
//   • Request deduplication (won't enqueue the same (category,id) twice)
//   • A background IconWorker running on its own QThread
//   • Delivery of completed icons via the iconReady() signal
// ---------------------------------------------------------------------------

class ThingIconProvider: public QObject
{
		Q_OBJECT

	public:
		static constexpr int ICON_SIZE = 64;
		static constexpr size_t DEFAULT_CACHE_LIMIT = 6000;

		explicit ThingIconProvider(QObject *parent = nullptr);
		~ThingIconProvider() override;

		/// Attach / detach the sprite file.  Changing the file invalidates
		/// all cached icons and cancels pending requests.
		void setSpriteFile(const SpriteFile *spriteFile);

		/// Return a cached icon for (category, thingId), or a null QIcon if
		/// the icon has not been built yet.
		QIcon getCachedIcon(ThingCategory category, uint16_t thingId) const;

		/// Request an icon.  If already cached the cached icon is returned
		/// immediately.  Otherwise a background request is enqueued and a
		/// null QIcon is returned — the caller should use placeholderIcon()
		/// until iconReady() fires for this (category, thingId).
		QIcon requestIcon(ThingCategory category, uint16_t thingId, const ThingType *thing);

		/// Convenience: build an IconBuildRequest from a ThingType snapshot.
		static IconBuildRequest makeBuildRequest(ThingCategory category, uint16_t thingId, const ThingType *thing);

		/// Cancel all pending background requests.
		void cancelAll();

		/// Cancel pending requests for a specific category.
		void cancelCategory(ThingCategory category);

		/// Remove a single entry from the cache (e.g. after a sprite edit).
		void invalidate(ThingCategory category, uint16_t thingId);

		/// Remove all entries for a category.
		void invalidateCategory(ThingCategory category);

		/// Remove all cached icons.
		void invalidateAll();

		/// Insert an icon directly into the cache (used for synchronous
		/// single-item rebuilds after an edit).
		void insertIcon(ThingCategory category, uint16_t thingId, const QIcon &icon);

		/// Shared placeholder icon (built once on first call).
		const QIcon &placeholderIcon();

		/// Current number of cached icons.
		size_t cacheSize() const;

	signals:
		/// Emitted on the main thread when a background icon build completes.
		void iconReady(ThingCategory category, uint16_t thingId, QIcon icon);

	private slots:
		void onImageReady(ThingCategory category, uint16_t thingId, QImage image);

	private:
		// --- Cache key --------------------------------------------------
		struct CacheKey
		{
				ThingCategory category;
				uint16_t thingId;
				bool operator==(const CacheKey &other) const
				{
					return category == other.category && thingId == other.thingId;
				}
		};

		struct CacheKeyHash
		{
				size_t operator()(const CacheKey &k) const
				{
					return std::hash<uint32_t>()(
					    (static_cast<uint32_t>(k.category) << 16) | k.thingId
					);
				}
		};

		// --- LRU cache --------------------------------------------------
		using LruList = std::list<CacheKey>;
		LruList m_lruList;

		struct CacheEntry
		{
				QIcon icon;
				LruList::iterator lruIter;
		};

		std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> m_cache;
		size_t m_cacheLimit = DEFAULT_CACHE_LIMIT;

		void touchCache(const CacheKey &key);
		void evictIfNeeded();

		// --- Pending set (for deduplication) ----------------------------
		std::unordered_set<CacheKey, CacheKeyHash> m_pending;

		// --- Worker thread ----------------------------------------------
		QThread *m_workerThread = nullptr;
		IconWorker *m_worker = nullptr;
		const SpriteFile *m_spriteFile = nullptr;

		void ensureWorkerThread();
		void stopWorkerThread();

		// --- Placeholder icon -------------------------------------------
		QIcon m_placeholderIcon;
		bool m_placeholderBuilt = false;
		void buildPlaceholderIcon();
};