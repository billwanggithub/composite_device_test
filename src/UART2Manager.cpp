#include "UART2Manager.h"
#include "driver/gpio.h"

UART2Manager::UART2Manager() {
}

UART2Manager::~UART2Manager() {
    end();
}

bool UART2Manager::begin(uint32_t baudRate, uart_stop_bits_t stopBits,
                         uart_parity_t parity, uart_word_length_t dataBits,
                         uint16_t txBufferSize, uint16_t rxBufferSize) {
    // Validate parameters
    if (!validateConfig(baudRate, stopBits, parity, dataBits)) {
        return false;
    }

    // If already initialized, shutdown first
    if (initialized) {
        end();
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = (int)baudRate,
        .data_bits = dataBits,
        .parity = parity,
        .stop_bits = stopBits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    // Configure UART parameters
    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        Serial.printf("[UART2] uart_param_config failed: %d\n", err);
        return false;
    }

    // Set UART pins (TX with pull-up, RX with pull-up for noise immunity)
    err = uart_set_pin(uartNum, PIN_UART2_TX, PIN_UART2_RX,
                      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        Serial.printf("[UART2] uart_set_pin failed: %d\n", err);
        return false;
    }

    // Configure TX pin with internal pull-up
    gpio_set_pull_mode((gpio_num_t)PIN_UART2_TX, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)PIN_UART2_RX, GPIO_PULLUP_ONLY);

    // Install UART driver with buffers
    err = uart_driver_install(uartNum, rxBufferSize, txBufferSize, 0, NULL, 0);
    if (err != ESP_OK) {
        Serial.printf("[UART2] uart_driver_install failed: %d\n", err);
        return false;
    }

    // Save configuration
    currentBaudRate = baudRate;
    currentStopBits = stopBits;
    currentParity = parity;
    currentDataBits = dataBits;
    txBufSize = txBufferSize;
    rxBufSize = rxBufferSize;
    initialized = true;

    Serial.printf("[UART2] Initialized: %u baud, %d data bits, %d stop bits\n",
                  baudRate, dataBits + 5, stopBits + 1);

    return true;
}

void UART2Manager::end() {
    if (!initialized) {
        return;
    }

    uart_driver_delete(uartNum);
    initialized = false;

    Serial.println("[UART2] Shutdown complete");
}

bool UART2Manager::reconfigure(uint32_t baudRate, uart_stop_bits_t stopBits,
                               uart_parity_t parity, uart_word_length_t dataBits) {
    if (!initialized) {
        return false;
    }

    // Validate parameters
    if (!validateConfig(baudRate, stopBits, parity, dataBits)) {
        return false;
    }

    // Flush TX buffer before reconfiguration
    uart_wait_tx_done(uartNum, pdMS_TO_TICKS(1000));

    // Configure new parameters
    uart_config_t uart_config = {
        .baud_rate = (int)baudRate,
        .data_bits = dataBits,
        .parity = parity,
        .stop_bits = stopBits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        Serial.printf("[UART2] Reconfigure failed: %d\n", err);
        return false;
    }

    // Update current settings
    currentBaudRate = baudRate;
    currentStopBits = stopBits;
    currentParity = parity;
    currentDataBits = dataBits;

    Serial.printf("[UART2] Reconfigured: %u baud, %d data bits, %d stop bits\n",
                  baudRate, dataBits + 5, stopBits + 1);

    return true;
}

int UART2Manager::write(const uint8_t* data, size_t len, uint32_t timeoutMs) {
    if (!initialized || data == nullptr || len == 0) {
        return -1;
    }

    int written = uart_write_bytes(uartNum, (const char*)data, len);
    if (written > 0) {
        totalTxBytes += written;
    } else {
        errorCount++;
    }

    // Optional: Wait for transmission to complete
    if (timeoutMs > 0) {
        uart_wait_tx_done(uartNum, pdMS_TO_TICKS(timeoutMs));
    }

    return written;
}

int UART2Manager::write(const char* str, uint32_t timeoutMs) {
    if (str == nullptr) {
        return -1;
    }
    return write((const uint8_t*)str, strlen(str), timeoutMs);
}

int UART2Manager::read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) {
    if (!initialized || buffer == nullptr || maxLen == 0) {
        return -1;
    }

    int len = uart_read_bytes(uartNum, buffer, maxLen, pdMS_TO_TICKS(timeoutMs));
    if (len > 0) {
        totalRxBytes += len;
    } else if (len < 0) {
        errorCount++;
    }

    return len;
}

int UART2Manager::readLine(char* buffer, size_t maxLen, uint32_t timeoutMs) {
    if (!initialized || buffer == nullptr || maxLen < 2) {
        return -1;
    }

    size_t idx = 0;
    uint32_t startTime = millis();

    while (idx < maxLen - 1) {
        // Check timeout
        if (millis() - startTime >= timeoutMs) {
            buffer[idx] = '\0';
            return (idx > 0) ? idx : 0;  // Return 0 for timeout with no data
        }

        // Read one byte
        uint8_t c;
        int result = uart_read_bytes(uartNum, &c, 1, pdMS_TO_TICKS(10));

        if (result == 1) {
            totalRxBytes++;

            // Check for newline
            if (c == '\n') {
                buffer[idx++] = c;
                buffer[idx] = '\0';
                return idx;
            }
            // Skip \r (handle \r\n or \n)
            else if (c == '\r') {
                continue;
            }
            // Store character
            else {
                buffer[idx++] = c;
            }
        } else if (result < 0) {
            errorCount++;
            return -1;
        }
        // result == 0: No data yet, continue waiting
    }

    // Buffer full
    buffer[maxLen - 1] = '\0';
    return maxLen - 1;
}

int UART2Manager::available() {
    if (!initialized) {
        return 0;
    }

    size_t len = 0;
    uart_get_buffered_data_len(uartNum, &len);
    return (int)len;
}

bool UART2Manager::flush(uint32_t timeoutMs) {
    if (!initialized) {
        return false;
    }

    esp_err_t err = uart_wait_tx_done(uartNum, pdMS_TO_TICKS(timeoutMs));
    return (err == ESP_OK);
}

void UART2Manager::clearRxBuffer() {
    if (!initialized) {
        return;
    }

    uart_flush_input(uartNum);
}

void UART2Manager::getStatistics(uint32_t* txBytes, uint32_t* rxBytes, uint32_t* errors) {
    if (txBytes) *txBytes = totalTxBytes;
    if (rxBytes) *rxBytes = totalRxBytes;
    if (errors) *errors = errorCount;
}

void UART2Manager::resetStatistics() {
    totalTxBytes = 0;
    totalRxBytes = 0;
    errorCount = 0;
}

bool UART2Manager::isValidBaudRate(uint32_t baudRate) {
    // Validate baud rate range (2400 - 1,500,000)
    return (baudRate >= 2400 && baudRate <= 1500000);
}

bool UART2Manager::validateConfig(uint32_t baudRate, uart_stop_bits_t stopBits,
                                  uart_parity_t parity, uart_word_length_t dataBits) {
    // Validate baud rate
    if (!isValidBaudRate(baudRate)) {
        Serial.printf("[UART2] Invalid baud rate: %u (valid: 2400-1500000)\n", baudRate);
        return false;
    }

    // Validate stop bits
    if (stopBits < UART_STOP_BITS_1 || stopBits > UART_STOP_BITS_2) {
        Serial.printf("[UART2] Invalid stop bits: %d\n", stopBits);
        return false;
    }

    // Validate parity
    if (parity < UART_PARITY_DISABLE || parity > UART_PARITY_ODD) {
        Serial.printf("[UART2] Invalid parity: %d\n", parity);
        return false;
    }

    // Validate data bits
    if (dataBits < UART_DATA_5_BITS || dataBits > UART_DATA_8_BITS) {
        Serial.printf("[UART2] Invalid data bits: %d\n", dataBits);
        return false;
    }

    return true;
}
