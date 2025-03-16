// Cache name with version - increment this version to trigger the update process
const CACHE_NAME = 'panohead-v11';

// List of assets to cache for offline functionality
const ASSETS = [
    './',
    './index.html',
    './styles.css',
    './app.js',
    './manifest.json',
    './icons/android-chrome-192x192.png',
    './icons/android-chrome-512x512.png'
];

// Installation event: triggered when the service worker is first installed
// This caches all the essential assets for offline functionality
self.addEventListener('install', (event) => {
    event.waitUntil(
        caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
    );
});

// Activation event: triggered when a new service worker takes control
// This is responsible for cleaning up old caches
self.addEventListener('activate', (event) => {
    event.waitUntil(
        Promise.all([
            // Clean up old caches
            caches.keys().then((cacheNames) => {
                return Promise.all(
                    cacheNames.map((cacheName) => {
                        // Delete any cache that isn't the current version
                        if (cacheName !== CACHE_NAME) {
                            return caches.delete(cacheName);
                        }
                    })
                );
            })
        ])
    );
});

// Fetch event: intercepts all network requests
// Implements a "Cache first, then network" strategy
self.addEventListener('fetch', (event) => {
    event.respondWith(
        // First try to get the resource from cache
        caches.match(event.request).then((response) => {
            // Return cached response if found
            // If not in cache, fetch from network and cache the response
            return response || fetch(event.request).then(fetchResponse => {
                return caches.open(CACHE_NAME).then(cache => {
                    // Store a copy of the network response in the cache
                    cache.put(event.request, fetchResponse.clone());
                    // Return the network response
                    return fetchResponse;
                });
            });
        })
    );
});

// Message event: handles messages sent from the web app
// Used for update flow control
self.addEventListener('message', (event) => {
    // 'skipWaiting' message triggers immediate activation of the new service worker
    if (event.data === 'skipWaiting') {
        self.skipWaiting();
    }
});
