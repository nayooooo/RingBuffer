#include "../../src/RingBuffer.h"
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <intrin.h>  // For memory barrier intrinsics

// Test parameters
#define TOTAL_DATA_SIZE (1024ULL * 1024 * 1024 * 1024)  // 1TB
#define BUFFER_SIZE     (1024 * 1024)         // 1MB buffer
#define CHUNK_SIZE      (64   * 1024)         // 64KB chunk size for better throughput

// Global variables
static RingBuffer g_rb;
static HANDLE g_hProducerThread = NULL;
static HANDLE g_hConsumerThread = NULL;
static HANDLE g_hMonitorThread = NULL;

// Progress statistics (atomic for lock-free access)
static volatile uint64_t g_totalProduced = 0;
static volatile uint64_t g_totalConsumed = 0;
static volatile bool g_producerFinished = false;
static volatile bool g_consumerFinished = false;
static volatile bool g_stopMonitor = false;

// Data verification
static volatile uint32_t g_expectedPattern = 0;
static volatile uint32_t g_errors = 0;

// Performance counters
static LARGE_INTEGER g_frequency;
static LARGE_INTEGER g_startTime;
static LARGE_INTEGER g_endTime;

// Memory barrier functions (compiler and hardware barriers)
#define COMPILER_BARRIER() _ReadWriteBarrier()
#define MEMORY_BARRIER() MemoryBarrier()

// Generate test data chunk
void generate_test_data(uint8_t *data, uint32_t size, uint32_t pattern) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = (uint8_t)((pattern + i) & 0xFF);
    }
}

// Verify data
bool verify_data(const uint8_t *data, uint32_t size, uint32_t expected_pattern) {
    for (uint32_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((expected_pattern + i) & 0xFF);
        if (data[i] != expected) {
            return false;
        }
    }
    return true;
}

// Producer thread function (lock-free)
DWORD WINAPI producer_thread(LPVOID lpParam) {
    (void)lpParam;

    uint8_t *data_buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!data_buffer) {
        printf("Producer: Failed to allocate memory\n");
        return 1;
    }

    printf("Producer: Started producing data...\n");

    uint64_t local_produced = 0;
    uint32_t local_pattern = 0;

    while (local_produced < TOTAL_DATA_SIZE) {
        // Generate data
        generate_test_data(data_buffer, CHUNK_SIZE, local_pattern);

        uint32_t to_write = CHUNK_SIZE;

        // Try to write to buffer in a lock-free manner
        while (to_write > 0) {
            // Get current buffer length (atomic read)
            uint32_t buffer_len = RingBufferLenGet(&g_rb);
            uint32_t free_space = BUFFER_SIZE - buffer_len - 1;

            if (free_space == 0) {
                // Buffer full, yield to other threads
                SwitchToThread();
                continue;
            }

            uint32_t write_size = (to_write < free_space) ? to_write : free_space;
            if (write_size == 0) {
                SwitchToThread();
                continue;
            }

            // Write to buffer (RingBufferPut is thread-safe for single producer)
            uint32_t written = RingBufferPut(&g_rb, data_buffer + (CHUNK_SIZE - to_write), write_size);

            if (written > 0) {
                to_write -= written;
                local_produced += written;
                local_pattern += written;

                // Update global counter with memory barrier
                MEMORY_BARRIER();
                g_totalProduced = local_produced;
                COMPILER_BARRIER();
                g_expectedPattern = local_pattern;
            }
        }
    }

    free(data_buffer);

    // Signal completion with memory barrier
    MEMORY_BARRIER();
    g_producerFinished = true;
    COMPILER_BARRIER();

    printf("Producer: Finished, total produced %llu bytes\n", local_produced);

    return 0;
}

// Consumer thread function (lock-free)
DWORD WINAPI consumer_thread(LPVOID lpParam) {
    (void)lpParam;

    uint8_t *data_buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!data_buffer) {
        printf("Consumer: Failed to allocate memory\n");
        return 1;
    }

    printf("Consumer: Started consuming data...\n");

    uint64_t local_consumed = 0;
    uint32_t local_pattern = 0;
    uint32_t local_errors = 0;

    while (local_consumed < TOTAL_DATA_SIZE) {
        uint32_t to_read = CHUNK_SIZE;

        // Try to read from buffer in a lock-free manner
        while (to_read > 0) {
            // Get current buffer length (atomic read)
            uint32_t buffer_len = RingBufferLenGet(&g_rb);

            if (buffer_len == 0) {
                // Check if producer is done
                MEMORY_BARRIER();
                if (g_producerFinished && buffer_len == 0) {
                    break;
                }

                // Buffer empty, yield to other threads
                SwitchToThread();
                continue;
            }

            uint32_t read_size = (to_read < buffer_len) ? to_read : buffer_len;

            // Read from buffer (RingBufferGet is thread-safe for single consumer)
            uint32_t read = RingBufferGet(&g_rb, data_buffer + (CHUNK_SIZE - to_read), read_size);

            if (read > 0) {
                // Verify data
                if (!verify_data(data_buffer + (CHUNK_SIZE - to_read), read, local_pattern)) {
                    local_errors++;
                }

                to_read -= read;
                local_consumed += read;
                local_pattern += read;

                // Update global counter with memory barrier
                MEMORY_BARRIER();
                g_totalConsumed = local_consumed;
                COMPILER_BARRIER();
            }
        }

        // Small sleep to prevent CPU hogging
        if (to_read == CHUNK_SIZE) {
            Sleep(0);
        }
    }

    free(data_buffer);

    // Update error count
    MEMORY_BARRIER();
    g_errors = local_errors;
    COMPILER_BARRIER();

    // Signal completion with memory barrier
    MEMORY_BARRIER();
    g_consumerFinished = true;
    COMPILER_BARRIER();

    printf("Consumer: Finished, total consumed %llu bytes, found %u errors\n", 
           local_consumed, local_errors);

    return 0;
}

// Monitor thread function
DWORD WINAPI monitor_thread(LPVOID lpParam) {
    (void)lpParam;

    printf("Monitor: Started, reporting status every second...\n");

    uint64_t last_produced = 0;
    uint64_t last_consumed = 0;
    LARGE_INTEGER last_report_time;

    QueryPerformanceCounter(&last_report_time);

    printf("\n\n\n\n\n\n\n\n\n\n\n");

    while (!g_stopMonitor) {
        Sleep(200);  // Report every 200 ms

        // Get current values with memory barriers
        MEMORY_BARRIER();
        uint64_t current_produced = g_totalProduced;
        uint64_t current_consumed = g_totalConsumed;
        uint32_t current_errors = g_errors;
        bool producer_done = g_producerFinished;
        bool consumer_done = g_consumerFinished;
        COMPILER_BARRIER();

        // Calculate throughput
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);

        double elapsed_seconds = (double)(current_time.QuadPart - last_report_time.QuadPart) / 
                                 g_frequency.QuadPart;

        double produce_rate = (current_produced - last_produced) / elapsed_seconds / (1024 * 1024);
        double consume_rate = (current_consumed - last_consumed) / elapsed_seconds / (1024 * 1024);

        // Get buffer status
        uint32_t buffer_used = RingBufferLenGet(&g_rb);
        uint32_t buffer_size = RingBufferSizeGet(&g_rb);
        float buffer_usage = (buffer_size > 0) ? 
            ((float)buffer_used / buffer_size * 100.0f) : 0.0f;

        // Calculate progress percentages
        float producer_progress = (TOTAL_DATA_SIZE > 0) ? 
            ((float)current_produced / TOTAL_DATA_SIZE * 100.0f) : 0.0f;
        float consumer_progress = (TOTAL_DATA_SIZE > 0) ? 
            ((float)current_consumed / TOTAL_DATA_SIZE * 100.0f) : 0.0f;

        for (uint32_t i = 0; i < 9; i++) {
            printf("\r\033[K\033[1A");
        }
        printf("\r\033[K");
        printf("=== Status Report ===\n");
        printf("Producer: %llu / %llu bytes (%.1f%%) at %.2f MB/s\n", 
               current_produced, TOTAL_DATA_SIZE, producer_progress, produce_rate);
        printf("Consumer: %llu / %llu bytes (%.1f%%) at %.2f MB/s\n", 
               current_consumed, TOTAL_DATA_SIZE, consumer_progress, consume_rate);
        printf("Buffer: %u / %u bytes (%.1f%% used)\n", 
               buffer_used, buffer_size, buffer_usage);
        printf("Verification errors: %u\n", current_errors);
        printf("Producer finished: %s\n", producer_done ? "Yes" : "No");
        printf("Consumer finished: %s\n", consumer_done ? "Yes" : "No");
        printf("====================\n\n");

        // Update for next iteration
        last_produced = current_produced;
        last_consumed = current_consumed;
        last_report_time = current_time;

        // Check if both threads are done
        if (producer_done && consumer_done) {
            g_stopMonitor = true;
        }
    }

    printf("Monitor: Exited\n");
    return 0;
}

// Calculate elapsed time in milliseconds
double get_elapsed_ms(LARGE_INTEGER start, LARGE_INTEGER end) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 / g_frequency.QuadPart;
}

int main() {
    printf("Lock-Free Ring Buffer Test Program\n");
    printf("Test size: %llu bytes (%llu GB)\n", TOTAL_DATA_SIZE, TOTAL_DATA_SIZE / (1024 * 1024 * 1024));
    printf("Buffer size: %u bytes\n", BUFFER_SIZE);
    printf("Chunk size: %u bytes\n", CHUNK_SIZE);
    printf("\n");

    // Get performance counter frequency
    QueryPerformanceFrequency(&g_frequency);

    // Create ring buffer
    printf("Initializing ring buffer...\n");
    int result = RingBufferCreate(&g_rb, BUFFER_SIZE);
    if (result != RB_OK) {
        printf("Failed to create ring buffer: %d\n", result);
        return 1;
    }
    printf("Ring buffer created successfully, size: %u\n", RingBufferSizeGet(&g_rb));

    // Record start time
    QueryPerformanceCounter(&g_startTime);

    // Create threads
    printf("Creating threads...\n");

    // Producer thread (higher priority to keep buffer filled)
    g_hProducerThread = CreateThread(
        NULL,
        0,
        producer_thread,
        NULL,
        CREATE_SUSPENDED,
        NULL
    );

    // Consumer thread
    g_hConsumerThread = CreateThread(
        NULL,
        0,
        consumer_thread,
        NULL,
        CREATE_SUSPENDED,
        NULL
    );

    // Monitor thread (lower priority)
    g_hMonitorThread = CreateThread(
        NULL,
        0,
        monitor_thread,
        NULL,
        CREATE_SUSPENDED,
        NULL
    );

    if (!g_hProducerThread || !g_hConsumerThread || !g_hMonitorThread) {
        printf("Failed to create threads\n");
        return 1;
    }

    // Set thread priorities
    SetThreadPriority(g_hProducerThread, THREAD_PRIORITY_ABOVE_NORMAL);
    SetThreadPriority(g_hConsumerThread, THREAD_PRIORITY_NORMAL);
    SetThreadPriority(g_hMonitorThread, THREAD_PRIORITY_BELOW_NORMAL);

    // Start threads
    ResumeThread(g_hProducerThread);
    ResumeThread(g_hConsumerThread);
    ResumeThread(g_hMonitorThread);

    // Wait for threads to complete
    printf("\nRunning test...\n");
    WaitForSingleObject(g_hProducerThread, INFINITE);
    WaitForSingleObject(g_hConsumerThread, INFINITE);

    // Record end time
    QueryPerformanceCounter(&g_endTime);

    // Wait for monitor to finish
    g_stopMonitor = true;
    WaitForSingleObject(g_hMonitorThread, 2000);

    // Cleanup
    printf("\nCleaning up resources...\n");
    RingBufferDelete(&g_rb);

    CloseHandle(g_hProducerThread);
    CloseHandle(g_hConsumerThread);
    CloseHandle(g_hMonitorThread);

    // Get final values with memory barriers
    MEMORY_BARRIER();
    uint64_t final_produced = g_totalProduced;
    uint64_t final_consumed = g_totalConsumed;
    uint32_t final_errors = g_errors;
    COMPILER_BARRIER();

    // Calculate performance metrics
    double total_time_ms = get_elapsed_ms(g_startTime, g_endTime);
    double total_time_sec = total_time_ms / 1000.0;
    double throughput_mbps = (final_consumed / total_time_sec) / (1024 * 1024);

    // Output final results
    printf("\n=== Final Results ===\n");
    printf("Total time: %.2f ms (%.3f seconds)\n", total_time_ms, total_time_sec);
    printf("Throughput: %.2f MB/s\n", throughput_mbps);
    printf("Total data produced: %llu bytes\n", final_produced);
    printf("Total data consumed: %llu bytes\n", final_consumed);
    printf("All data consumed: %s\n", (final_produced == final_consumed) ? "Yes" : "No");
    printf("Verification errors: %u\n", final_errors);

    if (final_errors == 0 && final_produced == TOTAL_DATA_SIZE && 
        final_consumed == TOTAL_DATA_SIZE) {
        printf("\nTest PASSED! Data accuracy: 100%%\n");
        printf("Performance: %.2f MB/s\n", throughput_mbps);
    } else {
        printf("\nTest FAILED!\n");
        if (final_errors > 0) {
            printf("  Reason: Found %u data verification errors\n", final_errors);
        }
        if (final_produced != TOTAL_DATA_SIZE) {
            printf("  Reason: Producer data amount incorrect\n");
        }
        if (final_consumed != TOTAL_DATA_SIZE) {
            printf("  Reason: Consumer data amount incorrect\n");
        }
    }

    return 0;
}