#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

/*
 * Volatile prototype storage. Non-volatile persistence can replace this module
 * later while keeping the same validation contract.
 */
typedef struct {
    char user_pin[USER_PIN_LENGTH + 1U];
    char admin_pin[ADMIN_PIN_LENGTH + 1U];
    uint32_t lockout_duration_ms;
    bool sound_enabled;
    uint8_t max_failed_attempts;
    uint8_t failed_attempts;
} Storage;

void storage_init_defaults(Storage *storage);
bool storage_set_user_pin(Storage *storage, const char *pin);
bool storage_set_admin_pin(Storage *storage, const char *pin);
bool storage_set_lockout_seconds(Storage *storage, uint32_t seconds);
void storage_increment_failed_attempts(Storage *storage);
void storage_reset_failed_attempts(Storage *storage);

#endif
