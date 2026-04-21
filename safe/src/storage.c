#include "storage.h"

#include <string.h>

static bool storage_is_fixed_digit_string(const char *pin, size_t length)
{
    size_t index;

    if (pin == NULL) {
        return false;
    }

    for (index = 0U; index < length; ++index) {
        if ((pin[index] < '0') || (pin[index] > '9')) {
            return false;
        }
    }

    return pin[length] == '\0';
}

void storage_init_defaults(Storage *storage)
{
    if (storage == NULL) {
        return;
    }

    (void)strcpy(storage->user_pin, "1234");
    (void)strcpy(storage->admin_pin, "654321");
    storage->lockout_duration_ms = DEFAULT_LOCKOUT_SECONDS * 1000U;
    storage->sound_enabled = true;
    storage->max_failed_attempts = MAX_FAILED_ATTEMPTS;
    storage->failed_attempts = 0U;
}

bool storage_set_user_pin(Storage *storage, const char *pin)
{
    if ((storage == NULL) || !storage_is_fixed_digit_string(pin, USER_PIN_LENGTH)) {
        return false;
    }

    (void)strcpy(storage->user_pin, pin);
    return true;
}

bool storage_set_admin_pin(Storage *storage, const char *pin)
{
    if ((storage == NULL) || !storage_is_fixed_digit_string(pin, ADMIN_PIN_LENGTH)) {
        return false;
    }

    (void)strcpy(storage->admin_pin, pin);
    return true;
}

bool storage_set_lockout_seconds(Storage *storage, uint32_t seconds)
{
    if ((storage == NULL) ||
        (seconds < MIN_LOCKOUT_SECONDS) ||
        (seconds > MAX_LOCKOUT_SECONDS)) {
        return false;
    }

    storage->lockout_duration_ms = seconds * 1000U;
    return true;
}

void storage_increment_failed_attempts(Storage *storage)
{
    if ((storage == NULL) || (storage->failed_attempts == UINT8_MAX)) {
        return;
    }

    storage->failed_attempts++;
}

void storage_reset_failed_attempts(Storage *storage)
{
    if (storage == NULL) {
        return;
    }

    storage->failed_attempts = 0U;
}
