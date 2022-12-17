#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>

#include "printer.h"

#define IO_CLK 14 // D5
#define IO_INP 12 // D6
#define IO_OUT 13 // D7

void ICACHE_FLASH_ATTR init_net(void);

printer_t printer = {
    .state = SYNC1,
    .cmd = NONE,

    .inp = 0,
    .out = 0,

    .bit = 0,

    .payload_len = 0,

    .image = { 0 },
    .image_pos = 0,
};

void ICACHE_FLASH_ATTR reset() {
    os_printf("Printer Error!");

    printer.state       = SYNC1;
    printer.image_pos   = 0;
    printer.payload_len = 0;
}

void ICACHE_FLASH_ATTR update() {
    switch (printer.state) {
    case SYNC1:
        if (printer.inp != 0x88) reset();
        else printer.state = SYNC2;

        break;
    case SYNC2:
        if (printer.inp != 0x33) reset();
        else printer.state = CMD;

        break;
    case CMD:
        printer.cmd = printer.inp;

        if (printer.cmd == INIT)
            printer.image_pos = 0;

        printer.state = COMPRESS;
        break;
    case COMPRESS:
        printer.state = LEN1;
        break;
    case LEN1:
        printer.payload_len |= printer.inp;

        printer.state = LEN2;
        break;
    case LEN2:
        printer.payload_len |= printer.inp << 8;

        printer.state = printer.payload_len == 0 ? CHECKSUM1 : PAYLOAD;
        break;
    case PAYLOAD:
        printer.payload_len--;

        if (printer.cmd == DATA)
            printer.image[printer.image_pos++] = printer.inp;
        // else if (printer.cmd == PRINT) {
        //     os_printf("PRINT: %d\n", printer.inp);
        // }

        printer.state = printer.payload_len == 0 ? CHECKSUM1 : PAYLOAD;
        break;
    case CHECKSUM1:
        printer.state = CHECKSUM2;
        break;
    case CHECKSUM2:
        printer.out = 0x81;

        printer.state = ACK;
        break;
    case ACK:
        printer.out = 0;

        printer.state = STAT;
        break;
    case STAT:
        os_printf("Read: %d bytes of image!\n", printer.image_pos);

        printer.state = SYNC1;
        break;
    }
}

void ICACHE_FLASH_ATTR clock_interrupt(void *data) {
    // Clear current interrupt
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, GPIO_REG_READ(GPIO_STATUS_ADDRESS) & BIT(IO_CLK));

    printer.inp |= GPIO_INPUT_GET(GPIO_ID_PIN(IO_OUT)) << (7 - printer.bit);

    GPIO_REG_WRITE(
        (printer.out & (0b10000000 >> printer.bit) ? GPIO_OUT_W1TS_ADDRESS : GPIO_OUT_W1TC_ADDRESS), 
        1 << IO_INP
    );

    printer.bit++;

    if (printer.bit == 8) {
        update();

        printer.bit = 0;
        printer.inp = 0;
    }
}

void ICACHE_FLASH_ATTR main(void) {
    gpio_init();

    // Initialize pins to GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);

    // Set IO_CLK & IO_OUT to input
    GPIO_DIS_OUTPUT(GPIO_ID_PIN(IO_CLK));
    GPIO_DIS_OUTPUT(GPIO_ID_PIN(IO_OUT));

    // Set IO_INP to output
    GPIO_OUTPUT_SET(GPIO_ID_PIN(IO_INP), 0);

    ETS_GPIO_INTR_DISABLE();

    ETS_GPIO_INTR_ATTACH(clock_interrupt, NULL);

    gpio_pin_intr_state_set(GPIO_ID_PIN(IO_CLK), GPIO_PIN_INTR_NEGEDGE);

    ETS_GPIO_INTR_ENABLE();

    init_net();
}
