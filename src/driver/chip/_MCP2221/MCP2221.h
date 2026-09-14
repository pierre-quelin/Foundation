/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MCP2221.h
 * @brief Microchip MCP2221(A) USB 2.0 to I2C/UART Protocol Converter with GPIO
 */
#pragma once

#include "protocol/i2c/I2C.h"
#include "protocol/usb/USB.h"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/signal/Signal.hpp"
#include "util/logger/Logger.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace mcp2221a
{
namespace hid
{
class Device;
} // namespace hid
} // namespace mcp2221a

namespace tools::design
{
struct ApplicationServices;

namespace config
{
class Node;
} // namespace config
} // namespace tools::design

namespace driver::chip
{

class MCP2221 final : public tools::design::factory::IObject, public protocol::i2c::I2CMaster
{
public:
    ~MCP2221() override;
    MCP2221(tools::design::ApplicationServices& app,
            tools::design::config::Node node);
    MCP2221(const MCP2221&)            = delete;
    MCP2221& operator=(const MCP2221&) = delete;
    MCP2221(MCP2221&&)                 = delete;
    MCP2221& operator=(MCP2221&&)      = delete;

    int init() override;
    /**
     * @brief Read the GPIO registers and informs observers of any changes.
     *
     * @return 0 if successful, -1 otherwise.
     */
    int busCycleBegin() override;

    // I2C Master
    int setSpeed(I2CMaster::Speed speed) override;
    int read(uint8_t addr, uint8_t* buf, size_t len) override;
    int write(uint8_t addr, const uint8_t* buf, size_t len) override;

    // SMBus
    int readByte(uint8_t addr, uint8_t cmd, uint8_t& value) override;
    int readWord(uint8_t addr, uint8_t cmd, uint16_t& value) override;
    int readBlock(uint8_t addr, uint8_t cmd, std::vector<uint8_t>& values) override;
    int writeByte(uint8_t addr, uint8_t cmd, uint8_t value) override;
    int writeWord(uint8_t addr, uint8_t cmd, uint16_t value) override;
    int writeBlock(uint8_t addr, uint8_t cmd, const std::vector<uint8_t>& values) override;

    // ADC
    enum class GPPin
    {
        GP0 = 0,
        GP1 = 1,
        GP2 = 2,
        GP3 = 3,
    };
    enum class GPMode
    {
        GPIO,
        ADC,
        DAC,
        LED_I2C,
        USBCFG,
        CLK_OUT,
        SSPND,
        LED_URX,
        LED_UTX,
        IOC
    };
    enum class GPIODirection
    {
        OUTPUT          = 0,
        INPUT           = 1,
        NOT_GPIO        = 0xEE,
        DIR_NOT_SET     = 0xEF,
        LEAVE_UNCHANGED = 0xFF
    };

    /**
     * @brief Sets the GPIO mode
     *
     * @param pin The pin
     * @param mode The desired mode
     * @return 0 if successful, -1 otherwise.
     */
    int setGPPinMode(GPPin pin, GPMode mode);
    /**
     * @brief Sets GPIO pin direction
     *
     * @param pin The pin
     * @param dir The desired Direction
     * @return 0 if successful, -1 otherwise.
     */
    int setGPIODirection(GPPin pin, GPIODirection dir);

    enum class GPIOState
    {
        LOW             = 0,
        HIGH            = 1,
        NOT_GPIO        = 0xEE,
        DIR_NOT_SET     = 0xEF,
        LEAVE_UNCHANGED = 0xFF
    };
    /**
     * @brief Sets the state of the GPIO Output
     *
     * @param pin The pin
     * @param state The desired GPIOState
     * @return 0 if successful, -1 otherwise.
     */
    int setGPIOState(GPPin pin, GPIOState state);
    /**
     * @brief Gets the state of the GPIO Input
     *
     * @param pin The pin
     * @param state
     * @return 0 if successful, -1 otherwise.
     */
    int getGPIOState(GPPin pin, GPIOState& state);

    enum class GPVRef
    {
        VREF_VDD  = 0,
        VREF_1024 = 1,
        VREF_2048 = 2,
        VREF_4096 = 3
    };
    /**
     * @brief Sets the ADC VRef
     *
     * @param vref The desired VRef
     * @return 0 if successful, -1 otherwise.
     */
    int setADCVRef(GPVRef vref);
    /**
     * @brief Reads the ADC data for the specified analog pin
     *
     * @param pin The analog pin
     * @param adcData The ADC data read
     * @return 0 if successful, -1 otherwise.
     */
    int getADCData(GPPin pin, unsigned int& adcData);

    /**
     * @brief Sets the DAC VRef
     *
     * @param vref The desired VRef
     * @return 0 if successful, -1 otherwise.
     */
    int setDACVref(GPVRef vref);

    /**
     * @brief Sets the DAC value
     *
     * @param value Valid range is between 0 and 31.
     * @return 0 if successful, -1 otherwise.
     */
    int setDACValue(unsigned int value);

    /**
     * @brief Cycle Observers
     * @note For RAII pattern see boost::signals2::scoped_connection
     */
    tools::design::signal::Signal<void(std::array<unsigned int, 3>& /* ADC values, TODO GPIOs Values */)> update;

private:
    MCP2221(const protocol::usb::USB_ID& usbId,
            const std::shared_ptr<util::logger::Logger>& logger);

    [[nodiscard]] util::logger::Logger& logger() const { return *_logger; }

    std::shared_ptr<util::logger::Logger> _logger;
    std::unique_ptr<mcp2221a::hid::Device> _dev;
    unsigned int _vid;
    unsigned int _pid;

    enum class Assignment
    {
        GPIO           = 0b000,
        DEDICATED_FUNC = 0b001,
        ALT_FUNC_0     = 0b010,
        ALT_FUNC_1     = 0b011,
        ALT_FUNC_2     = 0b100,
        UNKNOWN        = 0b111
    };
    static const std::array<std::array<Assignment, 10>, 4> _pinMode2Assign;

    /** Runtime SRAM image matching GET SRAM response (Table 3-39, bytes 0–25). */
    std::array<std::uint8_t, 26U> _sramBytes{};

    /** Flags indicating which SRAM fields have been modified and need the "alter" bit set. */
    enum SramDirtyFlag : std::uint8_t
    {
        DirtyNone       = 0,
        DirtyClockDiv   = 1 << 0,
        DirtyDacVref    = 1 << 1,
        DirtyDacValue   = 1 << 2,
        DirtyAdcVref    = 1 << 3,
        DirtyInterrupt  = 1 << 4,
        DirtyGpSettings = 1 << 5,
    };
    std::uint8_t _sramDirty{DirtyNone};

    /** I²C clock divider byte D (12 MHz / (D+2)); default ≈ 400 kbit/s (D=28). Kept as raw byte so MCP2221.h does not include libusb/windows headers (ERROR macro vs LogLevel::ERROR). */
    std::uint8_t _i2cDividerByte{28U};

    std::array<std::uint8_t, 4> _pinModes;
    std::array<std::uint8_t, 4> _pinDirections;
    std::array<std::uint8_t, 4> _pinValues;
    std::uint8_t _adcVref;
    std::uint8_t _dacVref;
    std::array<unsigned int, 3> _adcDatas;
    std::uint8_t _dacValue;

    std::recursive_mutex _mutex; // Only once for all at first. Recursive for cycle() -> close()

    /** Read SRAM into `_sramBytes` without logging or closing the device. */
    bool loadSramFromDevice();
    bool syncSramFromDevice();
    bool applySramSettings();
    void hidFailure(const char* context);

    /**
     * @brief Close the device and reset the handle
     */
    void close();
};

} // namespace driver::chip
