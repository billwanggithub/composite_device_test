#ifndef UART2_MANAGER_H
#define UART2_MANAGER_H

#include <Arduino.h>
#include "driver/uart.h"
#include "PeripheralPins.h"

/**
 * @brief UART2 Manager - Standard UART communication interface
 *
 * Provides a configurable UART interface with:
 * - Baud rate: 2400 - 1,500,000 bps
 * - Configurable stop bits (1, 1.5, 2)
 * - Configurable parity (None, Even, Odd)
 * - Configurable data bits (5, 6, 7, 8)
 * - TX/RX buffering with DMA
 * - Non-blocking read/write operations
 *
 * Hardware:
 * - GPIO 43: UART2 TX (with internal pull-up)
 * - GPIO 44: UART2 RX
 *
 * Usage:
 *   UART2Manager uart2;
 *   uart2.begin(115200, UART_STOP_BITS_1, UART_PARITY_DISABLE);
 *   uart2.write("Hello\n", 6);
 *   uint8_t buf[128];
 *   int len = uart2.read(buf, 128);
 */
class UART2Manager {
public:
    /**
     * @brief Constructor
     */
    UART2Manager();

    /**
     * @brief Destructor
     */
    ~UART2Manager();

    /**
     * @brief Initialize UART2
     * @param baudRate Baud rate (2400 - 1,500,000)
     * @param stopBits Stop bits (UART_STOP_BITS_1, UART_STOP_BITS_1_5, UART_STOP_BITS_2)
     * @param parity Parity mode (UART_PARITY_DISABLE, UART_PARITY_EVEN, UART_PARITY_ODD)
     * @param dataBits Data bits (UART_DATA_5_BITS to UART_DATA_8_BITS)
     * @param txBufferSize TX buffer size in bytes (default: 1024)
     * @param rxBufferSize RX buffer size in bytes (default: 2048)
     * @return true if initialization successful
     */
    bool begin(uint32_t baudRate = 115200,
               uart_stop_bits_t stopBits = UART_STOP_BITS_1,
               uart_parity_t parity = UART_PARITY_DISABLE,
               uart_word_length_t dataBits = UART_DATA_8_BITS,
               uint16_t txBufferSize = 1024,
               uint16_t rxBufferSize = 2048);

    /**
     * @brief Shutdown UART2
     */
    void end();

    /**
     * @brief Reconfigure UART parameters (without full re-initialization)
     * @param baudRate New baud rate
     * @param stopBits New stop bits setting
     * @param parity New parity setting
     * @param dataBits New data bits setting
     * @return true if reconfiguration successful
     */
    bool reconfigure(uint32_t baudRate,
                     uart_stop_bits_t stopBits = UART_STOP_BITS_1,
                     uart_parity_t parity = UART_PARITY_DISABLE,
                     uart_word_length_t dataBits = UART_DATA_8_BITS);

    /**
     * @brief Write data to UART2
     * @param data Pointer to data buffer
     * @param len Number of bytes to write
     * @param timeoutMs Timeout in milliseconds (0 = non-blocking)
     * @return Number of bytes actually written, -1 on error
     */
    int write(const uint8_t* data, size_t len, uint32_t timeoutMs = 100);

    /**
     * @brief Write string to UART2
     * @param str String to write
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes written, -1 on error
     */
    int write(const char* str, uint32_t timeoutMs = 100);

    /**
     * @brief Read data from UART2
     * @param buffer Buffer to store received data
     * @param maxLen Maximum bytes to read
     * @param timeoutMs Timeout in milliseconds (0 = return immediately)
     * @return Number of bytes actually read, -1 on error
     */
    int read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 100);

    /**
     * @brief Read a line from UART2 (until \n or \r\n)
     * @param buffer Buffer to store line
     * @param maxLen Maximum buffer size
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes read (including newline), -1 on error, 0 on timeout
     */
    int readLine(char* buffer, size_t maxLen, uint32_t timeoutMs = 1000);

    /**
     * @brief Get number of bytes available in RX buffer
     * @return Number of bytes available
     */
    int available();

    /**
     * @brief Flush TX buffer (wait until all data is sent)
     * @param timeoutMs Timeout in milliseconds
     * @return true if flush successful
     */
    bool flush(uint32_t timeoutMs = 1000);

    /**
     * @brief Clear RX buffer
     */
    void clearRxBuffer();

    /**
     * @brief Get current baud rate
     * @return Current baud rate
     */
    uint32_t getBaudRate() const { return currentBaudRate; }

    /**
     * @brief Get current stop bits setting
     * @return Current stop bits
     */
    uart_stop_bits_t getStopBits() const { return currentStopBits; }

    /**
     * @brief Get current parity setting
     * @return Current parity
     */
    uart_parity_t getParity() const { return currentParity; }

    /**
     * @brief Get current data bits setting
     * @return Current data bits
     */
    uart_word_length_t getDataBits() const { return currentDataBits; }

    /**
     * @brief Check if UART2 is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

    /**
     * @brief Get UART statistics
     * @param txBytes Pointer to store total TX bytes (optional)
     * @param rxBytes Pointer to store total RX bytes (optional)
     * @param errors Pointer to store error count (optional)
     */
    void getStatistics(uint32_t* txBytes = nullptr, uint32_t* rxBytes = nullptr, uint32_t* errors = nullptr);

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

private:
    bool initialized = false;
    uart_port_t uartNum = UART_NUM_UART2;

    // Current configuration
    uint32_t currentBaudRate = 115200;
    uart_stop_bits_t currentStopBits = UART_STOP_BITS_1;
    uart_parity_t currentParity = UART_PARITY_DISABLE;
    uart_word_length_t currentDataBits = UART_DATA_8_BITS;
    uint16_t txBufSize = 1024;
    uint16_t rxBufSize = 2048;

    // Statistics
    uint32_t totalTxBytes = 0;
    uint32_t totalRxBytes = 0;
    uint32_t errorCount = 0;

    // Validation helpers
    bool isValidBaudRate(uint32_t baudRate);
    bool validateConfig(uint32_t baudRate, uart_stop_bits_t stopBits,
                       uart_parity_t parity, uart_word_length_t dataBits);
};

#endif // UART2_MANAGER_H
