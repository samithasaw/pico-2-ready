#include <pico/stdio.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h" // GPIO සපෝට් එක සඳහා එකතු කරන ලදී
#include "led.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "bus.h"
#include "exploit.h"
#include "usb.h"
#include "log.h"

#define BUTTON_PIN 9 // Push button එක සම්බන්ධ කර ඇති GPIO Pin එක

#if WITH_AUTO_MODE

#define WITH_AUTO_REBOOT            (0)
#define FAILURE_REBOOT_DELAY_SEC    (3)
#define SUCCESS_REBOOT_DELAY_SEC    (5)

#define CONNECTION_FAIL_SLEEP_MS    (500)

__attribute__((noreturn))
void fatal_failure(void) {
    led_set_state(LED_STATE_ERROR);

#if WITH_AUTO_REBOOT
    printf("\nfatal failure, rebooting in %d seconds\n", FAILURE_REBOOT_DELAY_SEC);

    sleep_ms(FAILURE_REBOOT_DELAY_SEC * 1000);

    watchdog_reboot(0, SRAM_END, 1);
    while (1) {}

#else
    printf("\nfatal failure, spinning forever\n");

    while (1) {
        sleep_ms(100);
    }
#endif
}

__attribute__((noreturn))
void do_auto(void) {
    while (true) {
        int ret = exploit_run();

        if (ret == -2) {
            sleep_ms(CONNECTION_FAIL_SLEEP_MS);
            usb_bus_reset_open_ep0();
            continue;
        }

        if (ret != 0) {
            fatal_failure();
        }

        break;
    }

#if WITH_AUTO_REBOOT
    printf("\nsuccess, rebooting in %d seconds\n", SUCCESS_REBOOT_DELAY_SEC);

    sleep_ms(SUCCESS_REBOOT_DELAY_SEC * 1000);

    watchdog_reboot(0, SRAM_END, 1);
    while (1) {}
#else
    printf("\nsuccess, spinning forever\n");

    while (1) {
        sleep_ms(100);
    }
#endif
}

#else

static void help(void) {
    printf("'e'\texploit\n");
    printf("'r'\tbus reset\n");
    printf("'p'\treboot\n");
    printf("'h'\thelp\n");
}

void do_shell(void) {
    char buf[2] = { 0 };

    printf("\nDEBUG build, starting command prompt\n");

    printf("\n");
    help();
    printf("\n");

    while (1) {
        printf("> ");

        char c = stdio_getchar();

        printf("%c\n", c);

        switch (c) {
            case 'r': {
                printf("Resetting the bus...\n");
                usb_bus_reset_open_ep0();
                break;
            }

            case 'e': {
                usb_bus_reset_open_ep0();
                int ret = exploit_run();

                if (ret == -2) {
                    printf("failed to discover a device\n");
                }

                break;
            }

            case 'p': {
                printf("Rebooting the Pico...\n");

                sleep_ms(100);
                watchdog_reboot(0, SRAM_END, 1);
                while (1) {}
            }

            case 'h': {
                help();
                break;
            }

            default: {
                printf("Unknown command '%s'\n", buf);
                break;
            }
        }
    }
}

#endif

// Button එක Press වන තෙක් Wait වන Function එක
void wait_for_button_press(void) {
    printf("Waiting for Push Button (GPIO 9) press...\n");
    
    // Button එක Press (GND වෙන තෙක්) Loop එකේ තියෙනවා
    while (gpio_get(BUTTON_PIN) != 0) {
        sleep_ms(10);
    }
    
    printf("Button Pressed! Starting process...\n");
    // Debounce delay
    sleep_ms(200);
}

int main(void) {
#if PICO_RP2350
    set_sys_clock_khz(156000, true);
#elif PICO_RP2040
    set_sys_clock_khz(120000, true);
#else
#error What is this MCU even?
#endif

    led_init();
    led_set_state(LED_STATE_BOOTING);

    stdio_init_all();

    // Push Button එක (GPIO 9) Initialize කිරීම
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); // Active LOW pull-up setup

    sleep_ms(2000);

    led_set_state(LED_STATE_IDLE);

    printf("\n============ %s v%s ============\n", PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);
    printf("built for %s, PIO USB @ GP%d/%d (D+/D-)\n\n", BOARD_NAME, PIO_USB_DP_PIN_DEFAULT, PIO_USB_DP_PIN_DEFAULT + 1);

    // Push Button එක ඔබන තෙක් මෙතන නතර වී සිටී
    wait_for_button_press();

    usb_start();
    usb_bus_init();
    usb_bus_wait_for_device();
    usb_bus_reset_open_ep0();

#if WITH_AUTO_MODE
    do_auto();
#else
    do_shell();
#endif
}
