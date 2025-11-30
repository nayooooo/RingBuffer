#include <stdio.h>
#include "../../src/RingBuffer.h"

#include <stdlib.h>
#include <time.h>

#define TEST_LOOP                           (1000000)

#define TEST_RINGBUFFER_SIZE                (1024)
#define TEST_BUFFER_SIZE                    (2048)

static RingBuffer rb;

static uint8_t put_buff[TEST_BUFFER_SIZE] = { 0 };
static uint8_t get_buff[TEST_BUFFER_SIZE] = { 0 };

static void printInfo(RingBuffer *rb, const char *tag)
{
    printf("%s\n", (tag != NULL) ? tag : "(no name)");
    printf("ring buffer len %u\n", RingBufferLenGet(rb));
    printf("ring buffer size %u\n", RingBufferSizeGet(rb));
    printf("ring buffer total in %llu\n", RingBufferTotalInGet(rb));
    printf("ring buffer total out %llu\n", RingBufferTotalOutGet(rb));
}

int main()
{
    int status;
    uint32_t len;
    uint32_t put_len;
    uint32_t get_len;
    uint32_t loop;
    uint32_t put_err_times;
    uint32_t get_err_times;
    uint32_t data_err_times;

    srand(time(NULL));

    status = RingBufferCreate(&rb, TEST_RINGBUFFER_SIZE);
    if (status) {
        printf("create ring buffer fail(%d)\n", status);
        return status;
    } else {
        printf("create ring buffer succ\n");
    }

    printInfo(&rb, "after create");

    loop = TEST_LOOP;
    put_err_times = 0;
    get_err_times = 0;
    data_err_times = 0;
    printf("\n\n");
    printf("loop=%u\n", loop);
    do {
        len = (uint32_t)rand() % (TEST_BUFFER_SIZE + 1);
        if (len <= 0) {
            len = 1;
        }
        for (uint32_t i = 0; i < len; i++) {
            put_buff[i] = (uint8_t)rand();
        }
        put_len = RingBufferPut(&rb, &put_buff[0], len);
        if (len < TEST_RINGBUFFER_SIZE) {
            if (put_len != len) {
                put_err_times++;
            }
        } else {
            if (put_len != TEST_RINGBUFFER_SIZE - 1) {
                put_err_times++;
            }
        }
        get_len = RingBufferGet(&rb, &get_buff[0], put_len);
        if (get_len != put_len) {
            get_err_times++;
        }
        if (memcmp(&put_buff[0], &get_buff[0], get_len)) {
            data_err_times++;
        }
        printf("\033[2K\r %u/%u, put succ %.2f%%, get succ %.2f%%, data succ %.2f%%",
               TEST_LOOP - loop + 1, TEST_LOOP,
               100 - 100.0 * put_err_times / (TEST_LOOP - loop + 1),
               100 - 100.0 * get_err_times / (TEST_LOOP - loop + 1),
               100 - 100.0 * data_err_times / (TEST_LOOP - loop + 1));
    } while (--loop);
    printf("\n\n");

    printInfo(&rb, "before delete");

    status = RingBufferDelete(&rb);
    if (status) {
        printf("delete ring buffer fail(%d)\n", status);
        return status;
    } else {
        printf("delete ring buffer succ\n");
    }

    return 0;
}
