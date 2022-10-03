#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include <stdlib.h>

extern void hid_task(void);
static inline void process_mouse_report(hid_mouse_report_t const *p_report);

const uint VPULSE_PIN = 16;
const uint VQPULSE_PIN = 14;
const uint HPULSE_PIN = 17;
const uint HQPULSE_PIN = 15;
const uint BUTTON1_PIN = 18;
const uint BUTTON2_PIN = 13;

int main()
{
    gpio_init(VPULSE_PIN);
    gpio_init(VQPULSE_PIN);
    gpio_init(HPULSE_PIN);
    gpio_init(HQPULSE_PIN);
    gpio_init(BUTTON1_PIN);
    gpio_init(BUTTON2_PIN);

    gpio_set_dir(VPULSE_PIN, GPIO_OUT);
    gpio_set_dir(VQPULSE_PIN, GPIO_OUT);
    gpio_set_dir(HPULSE_PIN, GPIO_OUT);
    gpio_set_dir(HQPULSE_PIN, GPIO_OUT);
    gpio_set_dir(BUTTON1_PIN, GPIO_OUT);
    gpio_set_dir(BUTTON2_PIN, GPIO_OUT);

    gpio_put(VPULSE_PIN, true);
    gpio_put(VQPULSE_PIN, true);
    gpio_put(HPULSE_PIN, true);
    gpio_put(HQPULSE_PIN, true);
    gpio_put(BUTTON1_PIN, false);
    gpio_put(BUTTON2_PIN, false);

    board_init();
    tusb_init();

    while(1) {
        tuh_task();
    }
}

CFG_TUSB_MEM_SECTION static hid_mouse_report_t usb_mouse_report;

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len){
    process_mouse_report( (hid_mouse_report_t const*) report ); 
}

void toggle(uint pin) {
    bool pin_value = gpio_get(pin);
    gpio_put(pin, !pin_value);
}

static inline void process_mouse_report(hid_mouse_report_t const *p_report) 
{
    gpio_put(BUTTON1_PIN, !(p_report->buttons & MOUSE_BUTTON_LEFT));
    gpio_put(BUTTON2_PIN, !(p_report->buttons & MOUSE_BUTTON_RIGHT));

    for (int i=0; i<100; i++) {
        if (abs(p_report->x) > i) {
            if (p_report->x < 0) {
                toggle(HQPULSE_PIN);
            } else {
                toggle(HPULSE_PIN);
            }
        }
        if (abs(p_report->y) > i) {
            if (p_report->y < 0) {
                toggle(VQPULSE_PIN);
            } else {
                toggle(VPULSE_PIN);
            }
        }

        busy_wait_us(50);

        if (abs(p_report->x) > i) {
            if (p_report->x < 0) {
                toggle(HPULSE_PIN);
            } else {
                toggle(HQPULSE_PIN);
            }
        }
        if (abs(p_report->y) > i) {
            if (p_report->y < 0) {
                toggle(VPULSE_PIN);
            } else {
                toggle(VQPULSE_PIN);
            }
        }

        busy_wait_us(50);
    }

    busy_wait_us(500);
}

void tuh_hid_mouse_mounted_cb(uint8_t dev_addr) {
    // application set-up
    printf("A Mouse device (address %d) is mounted\r\n", dev_addr);
}

void tuh_hid_mouse_unmounted_cb(uint8_t dev_addr) {
    // application tear-down
    printf("A Mouse device (address %d) is unmounted\r\n", dev_addr);
}

// invoked ISR context
void tuh_hid_mouse_isr(uint8_t dev_addr, xfer_result_t event) {
    (void) dev_addr;
    (void) event;
}

