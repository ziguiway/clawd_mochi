#include "memory_monitor.h"

#include <esp_heap_caps.h>

#include "logger.h"

namespace MemoryMonitor {

namespace {
void readHeap(size_t& freeBytes, size_t& largestBlock, size_t& minimumFree) {
    freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    minimumFree = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
}
}

void logSnapshot(const char* tag) {
    size_t freeBytes = 0;
    size_t largestBlock = 0;
    size_t minimumFree = 0;
    readHeap(freeBytes, largestBlock, minimumFree);
    LOG_INFO("Memory", "%s free=%u largest=%u min=%u",
             tag ? tag : "snapshot",
             static_cast<unsigned int>(freeBytes),
             static_cast<unsigned int>(largestBlock),
             static_cast<unsigned int>(minimumFree));
}

bool hasTlsHeadroom(const char* tag) {
    size_t freeBytes = 0;
    size_t largestBlock = 0;
    size_t minimumFree = 0;
    readHeap(freeBytes, largestBlock, minimumFree);
    const bool safe = freeBytes >= TLS_MIN_FREE_BYTES &&
                      largestBlock >= TLS_MIN_LARGEST_BLOCK_BYTES;
    if (!safe) {
        LOG_WARN("Memory",
                 "%s TLS 延后: free=%u largest=%u min=%u",
                 tag ? tag : "network",
                 static_cast<unsigned int>(freeBytes),
                 static_cast<unsigned int>(largestBlock),
                 static_cast<unsigned int>(minimumFree));
    }
    return safe;
}

}

