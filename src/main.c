#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "main.h"
#include <stdlib.h>
#include "hardware/pio.h"
#include "kclk.pio.h" // assembled from kclk.pio
#include "kdat.pio.h" // assembled from kdat.pio

extern void hid_task(void);
static inline void process_keyboard_report(hid_keyboard_report_t const *p_report);

const uint KDAT_PIN = 16;
const uint KCLK_PIN = 14;
const uint KCLK_PIO_PIN = 10;
const uint KDAT_PIO_PIN = 11;

void send_sync_pulse() {
    gpio_set_dir(KDAT_PIN, GPIO_OUT);
    gpio_put(KDAT_PIN, false);
    busy_wait_us(20);
    gpio_put(KCLK_PIN, false);
    busy_wait_us(20);
    gpio_put(KCLK_PIN, true);
    busy_wait_us(20);
    gpio_put(KDAT_PIN, true);
}

int wait_for_ack(long timeout) {
	// Set the KEY_DATA pin to input so we can listen for ack
	gpio_set_dir(KDAT_PIN, GPIO_IN);

	// wait for pin to go low, then high again
    for (long i=0; i<timeout; i++){
      if (!gpio_get(KDAT_PIN)){
        for (long j=0; j<timeout; j++){
          if (gpio_get(KDAT_PIN)){
            return 1;
          }
          busy_wait_us(1);
        }
        return -1;
      }
      busy_wait_us(1);
    }
    return -1;
}

void send_byte(unsigned char b) {
    gpio_set_dir(KDAT_PIN, GPIO_OUT);
    busy_wait_us(100);

    int positions[]= {6, 5, 4, 3, 2, 1, 0, 7};  // the order in which the bits are clocked out
    for (int i=0; i<8; i++){
      int this_bit = (b >> positions[i]) & 1;
      gpio_put(KDAT_PIN, !this_bit);
      busy_wait_us(20);
      gpio_put(KCLK_PIN, false);
      busy_wait_us(20);
      gpio_put(KCLK_PIN, true);
      busy_wait_us(10);
      if (i == 7){
        gpio_set_dir(KDAT_PIN, GPIO_IN);
      }
      busy_wait_us(40);
    }
}

void init_sequence(){
    while (true) {
        // pulse a '1' on the data line
        send_sync_pulse();

        // wait for ack within ~134 ms
        // then send init codes
        if (wait_for_ack(22000) > 0) {
            send_byte(0xFD); // Initiate power up key stream
            if (wait_for_ack(1000) > 0) {
                send_byte(0xFE); // terminate key stream
                if (wait_for_ack(1000) > 0) {
                    return;
                }
            }
        }
    }
}

uint kclk_sm;
uint kdat_sm;
PIO pio;

int main()
{
    // PIO test
    pio = pio0;
    uint kclk_offset = pio_add_program(pio, &kclk_program);
    kclk_sm = pio_claim_unused_sm(pio, true);
    kclk_program_init(pio, kclk_sm, kclk_offset, KCLK_PIO_PIN);
    
    uint kdat_offset = pio_add_program(pio, &kdat_program);
    kdat_sm = pio_claim_unused_sm(pio, true);
    kdat_program_init(pio, kdat_sm, kdat_offset, KDAT_PIO_PIN);

    gpio_init(KDAT_PIN);
    gpio_init(KCLK_PIN);

    gpio_set_dir(KDAT_PIN, GPIO_OUT);
    gpio_set_dir(KCLK_PIN, GPIO_OUT);

    gpio_put(KDAT_PIN, true);
    gpio_put(KCLK_PIN, true);

    board_init();
    tusb_init();
    init_sequence();  // loop here until computer responds to the init sequence

    while(1) {
        tuh_task();
    }
}

CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len){
    process_keyboard_report( (hid_keyboard_report_t const*) report );
}

unsigned char apply_updown(unsigned char code, int updown){
    // generate the amiga key code for the given keypress
    // set the key up / down bit
    // 0 means keydown on Amiga
    if (updown) {
        // clear the msb
        code = code & 0x7F;
    } else {
        // set the msb
        code = code | 0x80;
    }
    return code;
}

void check_for_reset(hid_keyboard_report_t const *report) {
    if (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL & KEYBOARD_MODIFIER_LEFTGUI & KEYBOARD_MODIFIER_RIGHTGUI)) {
        // ctrl-amiga-amiga pressed (ctrl, left-win, right-win)
        // TODO: check if A500 or A2000 and reset accordingly
        // A500: hold reset pin low for 500us and run init_sequence
        // A2000: hold KCLK low for 500ms and run init_sequence

        // Using PIO
        pio_sm_set_enabled(pio, kclk_sm, false); // disable clock SM
        pio_sm_set_pins(pio, kclk_sm, 0); // force KCLK low
        busy_wait_us(500000); // wait 500ms
        pio_sm_set_enabled(pio, kclk_sm, true); // start clock running again
        
        pio_sm_restart(pio, kdat_sm); // restart kdat state machine
    }
}

static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode){
  for(uint8_t i=0; i<6; i++){
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

void send_key_presses(hid_keyboard_report_t const *report){
    static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released
	//get all keys currently pressed -> keys_down_now (translate special keys into codes)
	unsigned char keys_down_now[13] = {0};
	int i = 0;

	if (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)){
		keys_down_now[i] = 0x63;
		i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_LEFTGUI)){
			keys_down_now[i] = 0x66;
			i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_RIGHTGUI)){
			keys_down_now[i] = 0x67;
			i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_LEFTALT)){
			keys_down_now[i] = 0x64;
			i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_RIGHTALT)){
			keys_down_now[i] = 0x65;
			i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT)){
			keys_down_now[i] = 0x60;
			i++;
	}
	if (report->modifier & (KEYBOARD_MODIFIER_RIGHTSHIFT)){
			keys_down_now[i] = 0x61;
			i++;
	}

	for (int j=0; j<6; j++){
		if (report->keycode[j] != 0){
			keys_down_now[i] = key_map[HID_KEYBRD_Codes[report->keycode[j]]];
			i++;
		}
	}

	//for any not in keys_down_before, send key down event
	/*int found;
	for (int k=0; k<13; k++){
		if (keys_down_now[k] != 0){
			found = 0;
			for (int l=0; l<13; l++){
				if (keys_down_now[k] == keys_down_before[l]){
					found = 1;
					break;
				}
			}
			if (found == 0){
				send_byte(apply_updown(keys_down_now[k], 1));
				//sprintf(message, "Down: %#02x", keys_down_now[k]);
				//debug_log(message);
			}
		}
	}

	//for any in keys_down_before and not in keys_down_now, send key up event
	for (int m=0; m<13; m++){
		if (keys_down_before[m] != 0){
			found = 0;
			for (int n=0; n<13; n++){
				if (keys_down_before[m] == keys_down_now[n]){
					found = 1;
					break;
				}
			}
			if (found == 0){
				send_byte(apply_updown(keys_down_before[m], 0));
				//sprintf(message, "Up: %#02x", keys_down_before[m]);
				//debug_log(message);
			}
		}
	}
*/
    prev_report = *report;
}

static inline void process_keyboard_report(hid_keyboard_report_t const *report) {
    // key was pressed.  do stuff
    check_for_reset(report);
    send_key_presses(report);
}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr) {
    // application set-up
    printf("A Keyboard device (address %d) is mounted\r\n", dev_addr);
}

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr) {
    // application tear-down
    printf("A Keyboard device (address %d) is unmounted\r\n", dev_addr);
}

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event) {
    (void) dev_addr;
    (void) event;
}

