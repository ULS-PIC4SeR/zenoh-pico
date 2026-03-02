#include <time.h>
#include <stdlib.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/system/platform.h"

/*------------------ Random ------------------*/
uint8_t z_random_u8(void) { return z_random_u32(); }

uint16_t z_random_u16(void) { return z_random_u32(); }

uint32_t z_random_u32(void) {
    uint32_t ret = random();
    return ret;
}

uint64_t z_random_u64(void) {
    uint64_t ret = 0;
    ret |= z_random_u32();
    ret = ret << 32;
    ret |= z_random_u32();
    return ret;
}

void z_random_fill(void *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        *((uint8_t *)buf) = z_random_u8();
    }
}

/*------------------ Memory ------------------*/
void *z_malloc(size_t size) { return malloc(size); }

void *z_realloc(void *ptr, size_t size) { return realloc(ptr, size); }

void z_free(void *ptr) { free(ptr); }

/*------------------ Sleep ------------------*/
z_result_t z_sleep_us(size_t time) {
    delayMicroseconds(time);
    return _Z_RES_OK;
}

z_result_t z_sleep_ms(size_t time) {
    delay(time);
    return _Z_RES_OK;
}

z_result_t z_sleep_s(size_t time) {
    delay(time * 1000);
    return _Z_RES_OK;
}

/*------------------ Clock ------------------*/
z_clock_t z_clock_now(void) { return millis(); }

unsigned long z_clock_elapsed_us(z_clock_t *instant) { return z_clock_elapsed_ms(instant) * 1000; }

unsigned long z_clock_elapsed_ms(z_clock_t *instant) {
    z_clock_t now = z_clock_now();

    unsigned long elapsed = (now - *instant);
    return elapsed;
}

unsigned long z_clock_elapsed_s(z_clock_t *instant) { return z_clock_elapsed_ms(instant) / 1000; }

void z_clock_advance_us(z_clock_t *clock, unsigned long duration) { z_clock_advance_ms(clock, duration / 1000); }

void z_clock_advance_ms(z_clock_t *clock, unsigned long duration) {;
    *clock += duration;
}

void z_clock_advance_s(z_clock_t *clock, unsigned long duration) { z_clock_advance_ms(clock, duration * 1000); }

/*------------------ Time ------------------*/
z_time_t z_time_now(void) {
    z_clock_t t = z_clock_now();
    z_time_t now;
    now.sec = t / 1000; // Convert milliseconds to seconds
    now.usec = (t % 1000) * 1000; // Convert remaining milliseconds to microseconds
    return now;
}

const char *z_time_now_as_str(char *const buf, unsigned long buflen) {
    // Not implemented for ATmega2560 FreeRTOS
    return NULL;
}

unsigned long z_time_elapsed_us(z_time_t *time) {
    z_time_t now = z_time_now();
    unsigned long elapsed = (unsigned long)(1000000 * (now.sec - time->sec) + (now.usec - time->usec));
    return elapsed;
}

unsigned long z_time_elapsed_ms(z_time_t *time) {
    z_time_t now = z_time_now();
    unsigned long elapsed = (unsigned long)(1000 * (now.sec - time->sec) + (now.usec - time->usec) / 1000);
    return elapsed;
}

unsigned long z_time_elapsed_s(z_time_t *time) {
    z_time_t now = z_time_now();
    unsigned long elapsed = (unsigned long)(now.sec - time->sec);
    return elapsed;
}

z_result_t _z_get_time_since_epoch(_z_time_since_epoch *t) {
    z_time_t now = z_time_now();
    t->secs = now.sec;
    t->nanos = now.usec * 1000; // Convert microseconds to nanoseconds
    return _Z_RES_OK;
}