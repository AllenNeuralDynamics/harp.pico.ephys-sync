#include <cstring>
#include <harp_synchronizer.h>
#include <core_registers.h>
#include <reg_types.h>
#include <config.h>
#include <pico/divider.h>
#ifdef DEBUG
    #include <pico/stdlib.h> // for uart printing
    #include <cstdio> // for printf
#endif

/**
 * \brief Callback function to dispatch the current timestamp over UART and
 *  reschedule this function on the next second. Fires when the set alarm time
 *  takes place.
 * \details this function will reschedule itself to fire on the next whole
 *  second (in "Harp" time). Note that Harp time can change slightly upon every
 *  resynchronization pulse, so the future alarm time is always recomputed
 *  rather than set to fire on a fixed interval.
 */
int64_t dispatch_seconds_from_uart_cb(alarm_id_t id, void* user_data)
{
    uint64_t harp_time_us = HarpSynchronizer::time_us_64();
    // Truncate since spec only asks for a 4-byte representation of seconds.
    uint32_t harp_time_s = uint32_t(div_u64u64(harp_time_us, 1'000'000ULL));
    // The following works because it's little-endian.
    uart_write_blocking(uart0, (uint8_t*)&harp_time_s, sizeof(harp_time_s));
    // Note: above statement will block and slow down the update loop,
    //  but this should not be an issue since we don't need to poll for
    //  incoming Harp messages.
    // Reschedule:
    // Get next callback trigger time in pico time on the next whole second
    // in pico time.
    uint64_t alarm_time_us =
        HarpSynchronizer::harp_to_system_us_64(harp_time_s*1'000'000ULL)
        + 1'000'000ULL;
    // Reschedule this function.
    add_alarm_at(alarm_time_us, dispatch_seconds_from_uart_cb, nullptr, true);
    return 0; // indicate to not reschedule the alarm because we do so manually.
}


// Core0 main.
int main()
{
// Init Synchronizer.
    HarpSynchronizer::init(uart1, HARP_SYNC_RX_PIN);
#ifdef DEBUG
    stdio_uart_init_full(uart0, 921600, UART_TX_PIN, -1); // use uart1 tx only.
    printf("Hello, from an RP2040!\r\n");
#endif
    uart_init(uart0, EPHYS_UART_SYNC_BAUDRATE);
    uart_set_format(uart0, 32, 1, UART_PARITY_NONE);
    // Wait for at least one sync signal.
    while (!HarpSynchronizer::has_synced()){}
    // Schedule first callback on the next whole second (in harp time).
    uint64_t harp_time_us = HarpSynchronizer::time_us_64();
    // Round up to next second in harp time.
    uint64_t remainder;
    uint64_t quotient = divmod_u64u64_rem(harp_time_us, 1'000'000ULL,
                                          &remainder);
    uint64_t alarm_harp_time_us = harp_time_us + 1'000'000ULL - remainder;
    // Convert to pico time.
    uint64_t alarm_time_us =
        HarpSynchronizer::harp_to_system_us_64(alarm_harp_time_us);
    // Schedule the first uart dispatch, which will schedule all future ones.
    add_alarm_at(alarm_time_us, dispatch_seconds_from_uart_cb, nullptr, true);
    while(true)
    {}
}
