#include <cstring>
#include <harp_c_app.h>
#include <harp_synchronizer.h>
#include <core_registers.h>
#include <reg_types.h>
#include <config.h>
#ifdef DEBUG
    #include <pico/stdlib.h> // for uart printing
    #include <cstdio> // for printf
#endif


// Create device name array.
const uint16_t who_am_i = 1234;
const uint8_t hw_version_major = 0;
const uint8_t hw_version_minor = 0;
const uint8_t assembly_version = 0;
const uint8_t harp_version_major = 0;
const uint8_t harp_version_minor = 0;
const uint8_t fw_version_major = 0;
const uint8_t fw_version_minor = 0;
const uint16_t serial_number = 0xCAFE;

// Harp App Register Setup.
const size_t reg_count = 2;

// Define register contents.
#pragma pack(push, 1)
struct app_regs_t
{
    volatile uint8_t test_byte;  // app register 0
    volatile uint32_t test_uint; // app register 1
} app_regs;
#pragma pack(pop)

// Define register "specs."
RegSpecs app_reg_specs[reg_count]
{
    {(uint8_t*)&app_regs.test_byte, sizeof(app_regs.test_byte), U8},
    {(uint8_t*)&app_regs.test_uint, sizeof(app_regs.test_uint), U32}
};

// Define register read-and-write handler functions.
RegFnPair reg_handler_fns[reg_count]
{
    {&HarpCore::read_reg_generic, &HarpCore::write_reg_generic},
    {&HarpCore::read_reg_generic, &HarpCore::write_to_read_only_reg_error}
};

void app_reset(){}

void update_app_state(){}

// Create Harp App.
HarpCApp& app = HarpCApp::init(who_am_i, hw_version_major, hw_version_minor,
                               assembly_version,
                               harp_version_major, harp_version_minor,
                               fw_version_major, fw_version_minor,
                               serial_number, "Example C App",
                               &app_regs, app_reg_specs,
                               reg_handler_fns, reg_count, update_app_state,
                               app_reset);


int64_t dispatch_seconds_from_uart_cb(alarm_id_t id, void* user_data)
{
    // We reschedule the message every time references the current harp time
    // in case the external synchronizer has adjusted the time.
    uint64_t harp_time_us = app.harp_time_us_64();
    uint32_t harp_time_s = uint32_t(harp_time_us/1'000'000ULL); // truncate ok.
    // The following works because it's little-endian.
    uart_write_blocking(uart0, (uint8_t*)&harp_time_s, sizeof(harp_time_s));
    // Note: above statement will block and slow down the update loop.
    //  If this is a dealbreaker, we can use DMA to create a nonblocking write.
    // Reschedule:
    // Get next callback trigger time in pico time on the next whole second
    // in pico time.
    uint32_t alarm_time_us =
        HarpCore::harp_to_system_us(harp_time_s*1'000'000UL) + 1'000'000UL;
    // reschedule this function.
    add_alarm_at(alarm_time_us, dispatch_seconds_from_uart_cb, nullptr, true);
    return 0; // indicate to not reschedule the alarm because we do so manually.
}


// Core0 main.
int main()
{
// Init Synchronizer.
    HarpSynchronizer::init(uart1, HARP_SYNC_RX_PIN);
    app.set_synchronizer(&HarpSynchronizer::instance());
#ifdef DEBUG
    stdio_uart_init_full(uart0, 921600, UART_TX_PIN, -1); // use uart1 tx only.
    printf("Hello, from an RP2040!\r\n");
#endif
    uart_init(uart0, EPHYS_UART_SYNC_BAUDRATE);
    uart_set_format(uart0, 32, 1, UART_PARITY_NONE);
    // Wait for at least one sync signal.
    while (!HarpSynchronizer::instance().has_synced()){}
    // Schedule first callback on the next whole second (in harp time).
    uint64_t harp_time_us = app.harp_time_us_64();
    // Round up to next second in harp time.
    uint32_t remainder;
    uint32_t quotient = divmod_u32u32_rem(harp_time_us, 1'000'000UL, &remainder);
    uint32_t alarm_harp_time_us = harp_time_us + 1'000'000UL - remainder;
    // Convert to pico time.
    uint32_t alarm_time_us = HarpCore::harp_to_system_us(alarm_harp_time_us);
    // Schedule the first uart dispatch, which will schedule all future ones.
    add_alarm_at(alarm_time_us, dispatch_seconds_from_uart_cb, nullptr, true);
    while(true)
        app.run();
}
