#include <stdio.h>
#include <iostream>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/adc.h"

// Blue
#define LED0_PIN (21)
// Green
#define LED1_PIN (22)
// Phy Pin 21: I2C channel 0 - DAC data
#define I2C0_SDA_PIN (16) 
// Phy Pin 22: I2C channel 0 - DAC clock
#define I2C0_SCL_PIN (17) 
// Physical pin 31 - ADC input from analog section
#define ADC0_PIN (26)

using namespace std;

/*
Assuming the raw PCM data looks like this:

              High Byte                      Low Byte
| 7   6   5   4   3   2   1   0  |  7   6   5   4   3   2   1   0  |
  b15 b14 b13 b12 b11 b10 b9  b8    b7  b6  b5  b4  b3  b2  b1  b0

The MCP4725 can only deal with 12 bits of significance, so we'll 
ignore bits b3-b0 on the input (those might be zero anyhow). Using
the labeling from the MCP4725 datasheet we have these bits:

| d11 d10 d9  d8  d7  d6  d5  d4 |  d3  d2  d1  d0  0   0   0   0  |

Which is convenient because that is exactly the format that they
specify for the second and third byte of the transfer.

See https://ww1.microchip.com/downloads/en/devicedoc/22039d.pdf 
(page 25). The bits are aligned in the same way, once you 
consider
*/
static void play(uint16_t rawSample) {

    // This was measured to take 310ns

    // Go from 16-bit PCM -32768->32767 to 12-bit PCM 0->4095
    //uint16_t centeredSample = (sample + 32767);
    //uint16_t rawSample = centeredSample >> 4;

    i2c_hw_t *hw = i2c_get_hw(i2c_default);

    // Tx FIFO must not be full
    if (!(hw->status & I2C_IC_STATUS_TFNF_BITS)) {
        cout << "Failed" << endl;
        return;
    }

    // To create an output sample we need to write three words.  The STOP flag
    // is set on the last one.
    //
    // 0 0 0 | 0   1   0   x   x   0   0   x 
    // 0 0 0 | d11 d10 d09 d08 d07 d06 d05 d04
    // 0 1 0 | d03 d02 d01 d00 x   x   x   x
    //   ^
    //   |
    //   +------ STOP BIT!
    //
    hw->data_cmd = 0b000'0100'0000;
    hw->data_cmd = 0b000'0000'0000 | ((rawSample >> 4) & 0xff); // High 8 bits
    // STOP requested.  Data is low 4 bits of sample, padded on right with zeros
    hw->data_cmd = 0b010'0000'0000 | ((rawSample << 4) & 0xff); 
}

/*
Load command:

openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program hello-world.elf verify reset exit"

Minicom (for console, not the UART being tested):
minicom -b 115200 -o -D /dev/ttyACM0
*/
int main() {
 
    stdio_init_all();

    gpio_init(LED0_PIN);
    gpio_set_dir(LED0_PIN, GPIO_OUT);
       
    gpio_init(LED1_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);

    // Setup I2C
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    i2c_set_baudrate(i2c_default, 800000);

    // Get the ADC pin initialized
    adc_gpio_init(ADC0_PIN);
    adc_init();
    uint8_t adcChannel = 0;
    adc_select_input(adcChannel);    
    /*
    const uint32_t adcClockHz = 48000000;
    const uint32_t audioSampleRate = 8000;
    adc_fifo_setup(
        // Enable
        true,   
        // DREQ not enabled
        false,
        // DREQ threshold (but assuming this also applies to INT)
        1,
        // If enabled, bit 15 of the FIFO contains error flag for each sample
        false,
        // Shift FIFO contents to be one byte in size (for byte DMA) - enables 
        // DMA to byte buffers.
        false
    );
    // Divide clock to 8 kHz
    adc_set_clkdiv(adcClockHz / audioSampleRate);
    */

    cout << "Hello World!" << endl;

    // One-time initialization of the I2C channel
    i2c_hw_t *hw = i2c_get_hw(i2c_default);
    hw->enable = 0;
    hw->tar = 0x60;
    hw->enable = 1;

    // There is a voltage reference that prevents anything above 3.0
    float maxAdc = (3.05 * 4096.0) / 3.3; 
    float minAdc = 0;

    while (1) {
        sleep_ms(500);
        gpio_put(LED0_PIN, 1);
        gpio_put(LED1_PIN, 0);
        play((uint16_t)maxAdc);
        sleep_ms(1);
        cout << adc_read() << endl;
        sleep_ms(500);
        gpio_put(LED0_PIN, 0);
        gpio_put(LED1_PIN, 1);
        play((uint16_t)minAdc);
        sleep_ms(1);
        cout << adc_read() << endl;
    }
}

