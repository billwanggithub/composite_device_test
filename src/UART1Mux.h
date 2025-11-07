#ifndef UART1_MUX_H
#define UART1_MUX_H

#include <Arduino.h>
#include "driver/uart.h"
#include "driver/ledc.h"
#include "driver/pcnt.h"
#include "PeripheralPins.h"

/**
 * @brief UART1 Multiplexing Manager
 *
 * Manages UART1 with three operating modes:
 * 1. UART Mode: Normal UART communication (TX with pull-up, RX standard)
 * 2. PWM Mode: TX outputs PWM (1Hz-500kHz), RX inputs RPM signal (up to 20kHz)
 * 3. Disabled: Pins released
 *
 * Mode switching sequence:
 * 1. Disable current peripheral
 * 2. Reconfigure GPIO pins
 * 3. Initialize new peripheral
 * 4. Wait settling time (10ms)
 *
 * Hardware:
 * - GPIO 17: UART1 TX / LEDC PWM output
 * - GPIO 18: UART1 RX / PCNT frequency counter
 *
 * Usage:
 *   UART1Mux uart1;
 *
 *   // UART mode
 *   uart1.setModeUART(115200);
 *   uart1.write("Hello\n");
 *
 *   // PWM/RPM mode
 *   uart1.setModePWM_RPM();
 *   uart1.setPWMFrequency(1000);  // 1kHz PWM on TX
 *   float freq = uart1.getRPMFrequency();  // Read frequency on RX
 */
class UART1Mux {
public:
    /**
     * @brief Operating mode enumeration
     */
    enum Mode {
        MODE_DISABLED,      ///< Pins released
        MODE_UART,          ///< UART communication
        MODE_PWM_RPM        ///< TX=PWM output, RX=RPM input
    };

    /**
     * @brief Constructor
     */
    UART1Mux();

    /**
     * @brief Destructor
     */
    ~UART1Mux();

    // ========================================================================
    // Mode Control
    // ========================================================================

    /**
     * @brief Switch to UART mode
     * @param baudRate Baud rate (2400-1,500,000)
     * @param stopBits Stop bits configuration
     * @param parity Parity configuration
     * @param dataBits Data bits configuration
     * @return true if mode switch successful
     *
     * TX pin configured with internal pull-up.
     */
    bool setModeUART(uint32_t baudRate = 115200,
                     uart_stop_bits_t stopBits = UART_STOP_BITS_1,
                     uart_parity_t parity = UART_PARITY_DISABLE,
                     uart_word_length_t dataBits = UART_DATA_8_BITS);

    /**
     * @brief Switch to PWM/RPM mode
     * @return true if mode switch successful
     *
     * TX: LEDC PWM output (1Hz-500kHz)
     * RX: PCNT frequency counter (up to 20kHz)
     */
    bool setModePWM_RPM();

    /**
     * @brief Disable UART1 and release pins
     */
    void disable();

    /**
     * @brief Get current operating mode
     * @return Current mode
     */
    Mode getMode() const { return currentMode; }

    /**
     * @brief Get mode name as string
     * @return Mode name
     */
    const char* getModeName() const;

    // ========================================================================
    // UART Mode Functions (only work in MODE_UART)
    // ========================================================================

    /**
     * @brief Write data to UART (MODE_UART only)
     * @param data Data buffer
     * @param len Number of bytes
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes written, -1 on error
     */
    int write(const uint8_t* data, size_t len, uint32_t timeoutMs = 100);

    /**
     * @brief Write string to UART (MODE_UART only)
     * @param str String to write
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes written, -1 on error
     */
    int write(const char* str, uint32_t timeoutMs = 100);

    /**
     * @brief Read data from UART (MODE_UART only)
     * @param buffer Buffer to store data
     * @param maxLen Maximum bytes to read
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes read, -1 on error
     */
    int read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 100);

    /**
     * @brief Get number of bytes available in RX buffer (MODE_UART only)
     * @return Number of bytes available
     */
    int available();

    /**
     * @brief Clear RX buffer (MODE_UART only)
     */
    void clearRxBuffer();

    /**
     * @brief Reconfigure UART parameters without changing mode
     * @param baudRate New baud rate
     * @param stopBits New stop bits
     * @param parity New parity
     * @param dataBits New data bits
     * @return true if successful
     */
    bool reconfigureUART(uint32_t baudRate,
                        uart_stop_bits_t stopBits = UART_STOP_BITS_1,
                        uart_parity_t parity = UART_PARITY_DISABLE,
                        uart_word_length_t dataBits = UART_DATA_8_BITS);

    // ========================================================================
    // PWM/RPM Mode Functions (only work in MODE_PWM_RPM)
    // ========================================================================

    /**
     * @brief Set PWM frequency on TX pin (MODE_PWM_RPM only)
     * @param frequency Frequency in Hz (1 - 500,000)
     * @return true if successful
     */
    bool setPWMFrequency(uint32_t frequency);

    /**
     * @brief Set PWM duty cycle on TX pin (MODE_PWM_RPM only)
     * @param duty Duty cycle in percent (0.0 - 100.0)
     * @return true if successful
     */
    bool setPWMDuty(float duty);

    /**
     * @brief Get current PWM frequency
     * @return Current PWM frequency in Hz
     */
    uint32_t getPWMFrequency() const { return pwmFrequency; }

    /**
     * @brief Get current PWM duty cycle
     * @return Current duty cycle in percent
     */
    float getPWMDuty() const { return pwmDuty; }

    /**
     * @brief Enable/disable PWM output (MODE_PWM_RPM only)
     * @param enable true to enable, false to disable
     */
    void setPWMEnabled(bool enable);

    /**
     * @brief Check if PWM output is enabled
     * @return true if enabled
     */
    bool isPWMEnabled() const { return pwmEnabled; }

    /**
     * @brief Update RPM frequency measurement (MODE_PWM_RPM only)
     *
     * Should be called periodically (e.g., every 100ms) to update frequency reading.
     * Uses 100ms measurement window for accuracy.
     */
    void updateRPMFrequency();

    /**
     * @brief Get measured RPM frequency on RX pin (MODE_PWM_RPM only)
     * @return Frequency in Hz, 0 if no signal or not in PWM_RPM mode
     */
    float getRPMFrequency() const { return rpmFrequency; }

    /**
     * @brief Check if RPM signal is present
     * @return true if signal detected in last 500ms
     */
    bool hasRPMSignal() const;

    // ========================================================================
    // Status and Diagnostics
    // ========================================================================

    /**
     * @brief Get UART statistics (MODE_UART only)
     * @param txBytes Total transmitted bytes
     * @param rxBytes Total received bytes
     * @param errors Error count
     */
    void getUARTStatistics(uint32_t* txBytes, uint32_t* rxBytes, uint32_t* errors);

    /**
     * @brief Reset UART statistics
     */
    void resetUARTStatistics();

    /**
     * @brief Get UART baud rate
     * @return Current baud rate (0 if not in UART mode)
     */
    uint32_t getUARTBaudRate() const { return uartBaudRate; }

private:
    Mode currentMode = MODE_DISABLED;
    uart_port_t uartNum = UART_NUM_UART1;

    // UART mode state
    uint32_t uartBaudRate = 115200;
    uart_stop_bits_t uartStopBits = UART_STOP_BITS_1;
    uart_parity_t uartParity = UART_PARITY_DISABLE;
    uart_word_length_t uartDataBits = UART_DATA_8_BITS;
    uint32_t uartTxBytes = 0;
    uint32_t uartRxBytes = 0;
    uint32_t uartErrors = 0;

    // PWM mode state
    uint32_t pwmFrequency = 1000;      // Default 1kHz
    float pwmDuty = 50.0;              // Default 50%
    bool pwmEnabled = false;

    // RPM measurement state
    float rpmFrequency = 0.0;          // Measured frequency in Hz
    unsigned long lastRPMUpdate = 0;   // Last RPM update time
    int16_t lastPCNTCount = 0;         // Last PCNT counter value
    unsigned long rpmMeasureStartTime = 0;  // Measurement window start

    // Helper functions
    bool initUART();
    bool initPWM();
    bool initRPM();
    void deinitUART();
    void deinitPWM();
    void deinitRPM();
    void releasePins();
    bool validateUARTConfig(uint32_t baudRate, uart_stop_bits_t stopBits,
                           uart_parity_t parity, uart_word_length_t dataBits);
    bool validatePWMFrequency(uint32_t frequency);
};

#endif // UART1_MUX_H
