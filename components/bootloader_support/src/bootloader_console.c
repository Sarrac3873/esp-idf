// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"
#include "bootloader_console.h"
#include "soc/uart_periph.h"
#include "soc/uart_channel.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"
#include "soc/gpio_sig_map.h"
#include "soc/rtc.h"
#include "hal/clk_gate_ll.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/ets_sys.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#include "esp32s2/rom/usb/cdc_acm.h"
#include "esp32s2/rom/usb/usb_common.h"
#endif
#include "esp_rom_gpio.h"
#include "esp_rom_uart.h"

#ifdef CONFIG_ESP_CONSOLE_UART_NONE
void bootloader_console_init(void)
{
    ets_install_putc1(NULL);
    ets_install_putc2(NULL);
}
#endif // CONFIG_ESP_CONSOLE_UART_NONE

#ifdef CONFIG_ESP_CONSOLE_UART
void bootloader_console_init(void)
{
    const int uart_num = CONFIG_ESP_CONSOLE_UART_NUM;

    ets_install_uart_printf();

    // Wait for UART FIFO to be empty.
    esp_rom_uart_tx_wait_idle(0);

#if CONFIG_ESP_CONSOLE_UART_CUSTOM
    // Some constants to make the following code less upper-case
    const int uart_tx_gpio = CONFIG_ESP_CONSOLE_UART_TX_GPIO;
    const int uart_rx_gpio = CONFIG_ESP_CONSOLE_UART_RX_GPIO;
    // Switch to the new UART (this just changes UART number used for
    // ets_printf in ROM code).
    esp_rom_uart_set_as_console(uart_num);
    // If console is attached to UART1 or if non-default pins are used,
    // need to reconfigure pins using GPIO matrix
    if (uart_num != 0 ||
            uart_tx_gpio != UART_NUM_0_TXD_DIRECT_GPIO_NUM ||
            uart_rx_gpio != UART_NUM_0_RXD_DIRECT_GPIO_NUM) {
        // Change default UART pins back to GPIOs
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, PIN_FUNC_GPIO);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, PIN_FUNC_GPIO);
        // Route GPIO signals to/from pins
        const uint32_t tx_idx = uart_periph_signal[uart_num].tx_sig;
        const uint32_t rx_idx = uart_periph_signal[uart_num].rx_sig;
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[uart_rx_gpio]);
        esp_rom_gpio_pad_pullup_only(uart_rx_gpio);
        esp_rom_gpio_connect_out_signal(uart_tx_gpio, tx_idx, 0, 0);
        esp_rom_gpio_connect_in_signal(uart_rx_gpio, rx_idx, 0);
        // Enable the peripheral
        periph_ll_enable_clk_clear_rst(PERIPH_UART0_MODULE + uart_num);
    }
#endif // CONFIG_ESP_CONSOLE_UART_CUSTOM

    // Set configured UART console baud rate
    esp_rom_uart_set_clock_baudrate(uart_num, rtc_clk_apb_freq_get(), CONFIG_ESP_CONSOLE_UART_BAUDRATE);
}
#endif // CONFIG_ESP_CONSOLE_UART

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
/* Buffer for CDC data structures. No RX buffer allocated. */
static char s_usb_cdc_buf[ESP_ROM_CDC_ACM_WORK_BUF_MIN];

void bootloader_console_init(void)
{
#ifdef CONFIG_IDF_TARGET_ESP32S2
    /* ESP32-S2 specific patch to set the correct serial number in the descriptor.
     * Later chips don't need this.
     */
    rom_usb_cdc_set_descriptor_patch();
#endif

    esp_rom_uart_usb_acm_init(s_usb_cdc_buf, sizeof(s_usb_cdc_buf));
    esp_rom_uart_set_as_console(ESP_ROM_UART_USB);
    ets_install_putc1(bootloader_console_write_char_usb);
}
#endif //CONFIG_ESP_CONSOLE_USB_CDC