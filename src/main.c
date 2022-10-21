#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "main.h"
#include <stdlib.h>
#include "hardware/pio.h"
#include "keyboard.pio.h" // assembled from keyboard.pio
#include "amiga.pio.h" // assembles from amiga.pio

extern void hid_task(void);
static inline void process_keyboard_report(hid_keyboard_report_t const *p_report);

const uint LED_PIN = 25;
const uint KCLK_PIO_PIN = 10;
const uint KDAT_PIO_PIN = 11;
const uint AMIGA_KCLK_PIO_PIN = 12;
const uint AMIGA_KDAT_PIO_PIN = 13;

uint keyboard_sm;
uint amiga_sm;

void ack_received() {
    gpio_put(LED_PIN, true);
}

unsigned char rotate(unsigned char code) {
    // bits needs to be clocked out 6,5,4,3,2,1,0,7
    unsigned char bit7 = (code >> 7) & 0x01;
    return (code << 1) | bit7;
}

int main(){
    // IRQ 7 is PIO IRQ 0
    irq_set_exclusive_handler(7, ack_received);

    uint keyboard_offset = pio_add_program(pio0, &keyboard_program);
    keyboard_sm = pio_claim_unused_sm(pio0, true);
    keyboard_program_init(pio0, keyboard_sm, keyboard_offset, KCLK_PIO_PIN);
        
    // simulate an amiga - connect pins 10-12 and 11-13
    uint amiga_offset = pio_add_program(pio1, &amiga_program);
    amiga_sm = pio_claim_unused_sm(pio1, true);
    amiga_program_init(pio1, amiga_sm, amiga_offset, AMIGA_KCLK_PIO_PIN);

    // first bytes to be clocked out after init: TODO: handle loss of sync
    pio_sm_put(pio0, keyboard_sm, rotate(0xFD));  
    pio_sm_put(pio0, keyboard_sm, rotate(0xFE)); 

    // set up LED
    gpio_set_dir(LED_PIN, true);
    gpio_put(LED_PIN, false); // LED is off to start with

    board_init();
    tusb_init();

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

bool check_for_reset(hid_keyboard_report_t const *report) {
    if (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL & KEYBOARD_MODIFIER_LEFTGUI & KEYBOARD_MODIFIER_RIGHTGUI)) {
        // ctrl-amiga-amiga pressed (ctrl, left-win, right-win)
        // TODO: check if A500 or A2000 and reset accordingly
        // A500: hold reset pin low for 500us and run init_sequence
        // A2000: hold KCLK low for 500ms and run init_sequence
        pio_sm_set_enabled(pio0, keyboard_sm, false); // disable keyboard SM
        pio_sm_set_pins(pio0, keyboard_sm, 2); // force KCLK low, keep KDAT high
        busy_wait_us(500000); // wait 500ms
        gpio_put(LED_PIN, false);

        pio_sm_clear_fifos(pio0, keyboard_sm);
        pio_sm_put(pio0, keyboard_sm, 28500); // timeout, will be popped first when SM starts
        pio_sm_put(pio0, keyboard_sm, rotate(0xFD));  
        pio_sm_put(pio0, keyboard_sm, rotate(0xFE)); 
        pio_sm_set_enabled(pio0, keyboard_sm, true); // re-enable SM
        pio_sm_restart(pio0, keyboard_sm); // restart state machine
        return true;
    }
    return false;
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
				pio_sm_put_blocking(pio0, keyboard_sm, rotate(apply_updown(keys_down_now[k], 1)));
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
				pio_sm_put_blocking(pio0, keyboard_sm, rotate(apply_updown(keys_down_before[m], 0)));
			}
		}
	}
*/
    prev_report = *report;
}

static inline void process_keyboard_report(hid_keyboard_report_t const *report) {
    // key was pressed.  do stuff
    if (check_for_reset(report)) {
        return;
    }
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

