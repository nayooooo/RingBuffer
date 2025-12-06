#include <stdio.h>
#include <windows.h>
#include "../../src/RingBuffer.h"

#include <stdlib.h>
#include <time.h>

#define USE_DEBUG                           0

#ifndef BIT
#   define BIT(n) (1UL << (n))
#endif  // !BIT

#define TEST_TOTAL_BYTE                     (10000000)
#define TEST_DMA_BLOCK_SIZE                 (4095)
#define TEST_RINGBUFFER_SIZE                (2 * TEST_DMA_BLOCK_SIZE)

#define DMA_RECV_100KB_TIME_MS              (500)
#define TEST_DMA_RECV_100KB_TIME_MS         (DMA_RECV_100KB_TIME_MS)

#if (TEST_TOTAL_BYTE > 10000000)
#error too large!
#endif

#define CPU_THREAD_ATTR_CORE                (0)
#define DMA_THREAD_ATTR_CORE                (1)
#define MONITOR_THREAD_ATTR_CORE            (2)

static uint32_t monitor_thread_running = 0;
static uint32_t cpu_read_thread_running = 0;
static HANDLE monitor_thread = NULL;
static HANDLE threads[2] = { NULL, NULL };

static RingBuffer rb;

static RB_ADDRESS srcAddr = 0;
static RB_ADDRESS detAddr = 0;
static uint32_t blockSize = 0;
static uint32_t recvedLen = 0;

static uint8_t fifo[TEST_TOTAL_BYTE] = { 0 };
static volatile uint32_t dmaRecvTotalLen = 0;
static uint8_t readBuffer[TEST_TOTAL_BYTE] = { 0 };
static volatile uint32_t cpuReadToalLen = 0;
static volatile uint32_t errorLen = 0;

static void printInfo(RingBuffer *rb, const char *tag)
{
    if (tag != NULL) {
        printf("%s:\n", tag);
    } else {
        printf("(no name)\n");
    }
    printf("ring buffer len %u\n", RingBufferLenGet(rb));
    printf("ring buffer size %u\n", RingBufferSizeGet(rb));
    printf("ring buffer total in %llu\n", RingBufferTotalInGet(rb));
    printf("ring buffer total out %llu\n", RingBufferTotalOutGet(rb));
    printf("overflowTimes = %llu\n", RingBufferOverflowTimesGet(rb));
}

static void displaySystemInfo(void) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    printf("========== DMA simulator ==========\n");
    printf("system info:\n");
    printf("- CPU core number: %lu\n", sysInfo.dwNumberOfProcessors);
    printf("- buffer size: %d\n", TEST_RINGBUFFER_SIZE);
    printf("- total byte: %d\n", TEST_TOTAL_BYTE);
    printf("- dma block size max: %d\n", TEST_DMA_BLOCK_SIZE);
    printf("- CPU thread --> CPU core %d\n", CPU_THREAD_ATTR_CORE);
    printf("- DMA thread --> CPU core %d\n", DMA_THREAD_ATTR_CORE);
    printf("===================================\n\n");
}

static int dmaConfig(RB_ADDRESS src, RB_ADDRESS det, uint32_t size)
{
    srcAddr = src;
    detAddr = det;
    blockSize = size;
    return 0;
}

static uint32_t dmaRecvedLen(void)
{
    return recvedLen;
}

DWORD WINAPI cpu_thread(LPVOID lpParam)
{
    uint32_t len;
#if USE_DEBUG
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
#endif

    (void)lpParam;

#if USE_DEBUG
    QueryPerformanceCounter(&start);
#endif

    while (!cpu_read_thread_running) {
        Sleep(10);
    }
    printf("cpu thread start to read\n");

    while (cpuReadToalLen < TEST_TOTAL_BYTE) {
        len = RingBufferGet(&rb, &readBuffer[cpuReadToalLen], TEST_TOTAL_BYTE - cpuReadToalLen);
        if (len > 0) {
            cpuReadToalLen += len;
        } else {
            Sleep(1);
            continue;
        }

#if USE_DEBUG
        if (cpuReadToalLen >= 100000 && cpuReadToalLen < 105000) {
            QueryPerformanceCounter(&end);
            printf("\ncpu read 10 W byte use time: %.3lf ms\n",
                   (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart);
        }
#endif

        if (!cpu_read_thread_running) {
            break;
        }
    }
    if (memcmp(&readBuffer[0], &fifo[0], cpuReadToalLen)) {
        for (uint32_t i = 0; i < cpuReadToalLen; i++) {
            if (readBuffer[i] != fifo[i]) {
                errorLen++;
            }
        }
    }
    printf("\nerrorLen = %u\n", errorLen);

    return 0;
}

DWORD WINAPI dma_thread(LPVOID lpParam)
{
    int status;
#if USE_DEBUG
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
#endif

    (void)lpParam;

#if USE_DEBUG
    QueryPerformanceCounter(&start);
#endif

    printf("dma thread start to recv\n");

    while (dmaRecvTotalLen < TEST_TOTAL_BYTE) {
        if (dmaRecvTotalLen % TEST_DMA_BLOCK_SIZE == 0) {
            status = RingBufferDMAConfig(&rb, 1, TEST_DMA_BLOCK_SIZE);
            if (status) {
                printf("dma thread config dma fail(%d, %d, %u)\n", status, rb.dmaState, dmaRecvTotalLen);
                continue;
            }
            status = RingBufferDMAStart(&rb);
            if (status) {
                printf("dma thread start dma fail(%d, %d, %u)\n", status, rb.dmaState, dmaRecvTotalLen);
                continue;
            }
        }

        fifo[dmaRecvTotalLen] = (uint8_t)rand();
        ((uint8_t *)(detAddr))[recvedLen] = fifo[dmaRecvTotalLen];
        dmaRecvTotalLen++;
        recvedLen++;

        if (recvedLen == TEST_DMA_BLOCK_SIZE) {
            do {
                status = RingBufferDMAComplete(&rb);
                if (status) {
                    Sleep(0);
                }
            } while (status);
            recvedLen = 0;
            Sleep((DWORD)(1.0 * TEST_DMA_RECV_100KB_TIME_MS / (100000 / TEST_DMA_BLOCK_SIZE)));
        }

#if USE_DEBUG
        if (dmaRecvTotalLen == 100000) {
            QueryPerformanceCounter(&end);
            printf("\ndma recv 10 W byte use time: %.3lf ms\n",
                   (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart);
        }
#endif
    }
    RingBufferDMAStop(&rb);
    printf("\n");
    printf("dmaRecvTotalLen = %u\n", dmaRecvTotalLen);
    printf("recvedLen = %u\n", recvedLen);

    return 0;
}

DWORD WINAPI monitor_thread_entry(LPVOID lpParam)
{
    uint64_t last_delta = 0;
    uint64_t max_delta = 0;
    uint64_t min_delta = 0xFFFFFFFFFFFFFFFFULL;

    (void)lpParam;

    while (!monitor_thread_running) {
        Sleep(10);
    }

    do {
        printf("\033[2K\r delta(%u%s) %s, cpu %u/%u dma %u/%u",
               dmaRecvTotalLen - cpuReadToalLen,
               (dmaRecvTotalLen - cpuReadToalLen >= TEST_RINGBUFFER_SIZE) ? ", error" : "",
               (dmaRecvTotalLen - cpuReadToalLen > last_delta) ? "up" :
               (dmaRecvTotalLen - cpuReadToalLen < last_delta) ? "down" : "==",
               cpuReadToalLen, TEST_TOTAL_BYTE,
               dmaRecvTotalLen, TEST_TOTAL_BYTE);

        last_delta = dmaRecvTotalLen - cpuReadToalLen;
        if (min_delta > last_delta) {
            min_delta = last_delta;
        }
        if (max_delta < last_delta) {
            max_delta = last_delta;
        }

        Sleep(100);
    } while (monitor_thread_running);
    printf("\n cpu %u/%u dma %u/%u, errorLen %u, succ %.2f%%\n",
           cpuReadToalLen, TEST_TOTAL_BYTE, dmaRecvTotalLen, TEST_TOTAL_BYTE, errorLen,
           100.0 * (cpuReadToalLen - errorLen) / cpuReadToalLen);
    printf("delta: [%llu,%llu]\n", min_delta, max_delta);

    return 0;
}

int main(void)
{
    int status;

    status = (int)RingBufferLibraryBit();
#if _WIN64
    if (status != 64) {
#else
    if (status != 32) {
#endif
        printf("library = %d\n", status);
        return -1;
    }

    srand(time(NULL));

    displaySystemInfo();

    status = RingBufferCreate(&rb, TEST_RINGBUFFER_SIZE);
    if (status) {
        printf("create ring buffer fail(%d)\n", status);
        return status;
    } else {
        printf("create ring buffer succ\n");
    }
    printInfo(&rb, "after create");

    status = RingBufferDMADeviceRegister(&rb, dmaConfig, NULL, NULL, dmaRecvedLen, NULL, NULL);
    if (status) {
        printf("register dma device fail(%d)\n", status);
        return status;
    } else {
        printf("register dma device succ\n");
    }

    monitor_thread_running = 0;
    cpu_read_thread_running = 0;

    monitor_thread = CreateThread(NULL, 0, monitor_thread_entry, NULL, 0, NULL);
    if (monitor_thread == NULL) {
        printf("monitor thread create failed!\n");
        return -2;
    }
    printf("monitor thread created!\n");
    if (SetThreadAffinityMask(monitor_thread, BIT(MONITOR_THREAD_ATTR_CORE)) != 0) {
        printf("monitor thread put into core %d success!\n", MONITOR_THREAD_ATTR_CORE);
    } else {
        printf("monitor thread put into core %d failed!\n", MONITOR_THREAD_ATTR_CORE);
        return -3;
    }

    cpu_read_thread_running = 1;
    threads[0] = CreateThread(NULL, 0, cpu_thread, NULL, 0, NULL);
    if (threads[0] == NULL) {
        printf("cpu thread create failed!\n");
        return -2;
    }
    printf("cpu thread is created!\n");
    if (SetThreadAffinityMask(threads[0], BIT(CPU_THREAD_ATTR_CORE)) != 0) {
        printf("cpu thread put into core %d success!\n", CPU_THREAD_ATTR_CORE);
    } else {
        printf("cpu thread put into core %d failed!\n", CPU_THREAD_ATTR_CORE);
        return -3;
    }
    SetThreadPriority(threads[0], THREAD_PRIORITY_NORMAL);

    threads[1] = CreateThread(NULL, 0, dma_thread, NULL, 0, NULL);
    if (threads[1] == NULL) {
        printf("dma thread create failed!\n");
        return -2;
    }
    printf("dma thread is created!\n");
    if (SetThreadAffinityMask(threads[1], BIT(DMA_THREAD_ATTR_CORE)) != 0) {
        printf("dma thread put into core %d success!\n", DMA_THREAD_ATTR_CORE);
    } else {
        printf("dma thread put into core %d failed!\n", DMA_THREAD_ATTR_CORE);
        return -3;
    }
    SetThreadPriority(threads[1], THREAD_PRIORITY_ABOVE_NORMAL);

    monitor_thread_running = 1;

    printf("waitting for thread work complete...\n");
    WaitForMultipleObjects(1, &threads[1], TRUE, INFINITE);
    printf("dma thread complete!\n");
    status = (int)WaitForMultipleObjects(1, &threads[0], TRUE, 1000);
    if (status) {
        cpu_read_thread_running = 0;
        status = (int)WaitForMultipleObjects(1, &threads[0], TRUE, 1000);
        if (status) {
            printf("cpu thread timeout!\n");
            printf("error code: %d\n", status);
        }
    }
    printf("cpu thread complete!\n");

    monitor_thread_running = 0;
    status = (int)WaitForMultipleObjects(1, &monitor_thread, TRUE, 2000);
    if (status) {
        printf("monitor thread timeout!\n");
        printf("error code: %d\n", status);
    }
    printf("monitor thread complete!\n");

    CloseHandle(monitor_thread);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);

    printInfo(&rb, "before delete");
    status = RingBufferDelete(&rb);
    if (status) {
        printf("delete ring buffer fail(%d)\n", status);
        return status;
    } else {
        printf("delete ring buffer succ\n");
    }

    printf("test ok!\n");

    system("pause");

    return 0;
}
