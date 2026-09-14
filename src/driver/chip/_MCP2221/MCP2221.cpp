/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "driver/chip/MCP2221.h"

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"

#include "mcp2221a/hid/mcp2221a_hid.hpp"

#include <algorithm>
#include <array>
#include <chrono>

using namespace driver::chip;
using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace util::logger;
using util::chrono::literals::operator""_ms;

namespace
{

protocol::usb::USB_ID defaultUsbId()
{
    protocol::usb::USB_ID usbId;
    usbId._vid = 0x04D8; // Vendor ID
    usbId._pid = 0x00DD; // Product ID
    return usbId;
}

} // namespace

namespace
{

using mcp2221a::hid::CancelI2cTransfer;
using mcp2221a::hid::ConstByteSpan;
using mcp2221a::hid::Error;
using mcp2221a::hid::GpioAlterField;
using mcp2221a::hid::GpioOutputLevel;
using mcp2221a::hid::GpioPinCommandBlock;
using mcp2221a::hid::GpioPinDirection;
using mcp2221a::hid::GpioPinState;
using mcp2221a::hid::I2cClientAddress;
using mcp2221a::hid::I2cClockDivider;
using mcp2221a::hid::I2cRetryPolicy;
using mcp2221a::hid::MutableByteSpan;
using mcp2221a::hid::Report;
using mcp2221a::hid::SetI2cSpeedTag;
using mcp2221a::hid::StatusSnapshot;
namespace getSramIndex = mcp2221a::hid::getSramIndex;
namespace setSramIndex = mcp2221a::hid::setSramIndex;

const char* errorName(Error e)
{
    switch (e)
    {
        case Error::None:
            return "None";
        case Error::NotConnected:
            return "NotConnected";
        case Error::InvalidArgument:
            return "InvalidArgument";
        case Error::UsbTransfer:
            return "UsbTransfer";
        case Error::ProtocolMismatch:
            return "ProtocolMismatch";
        case Error::IncompleteRead:
            return "IncompleteRead";
        default:
            return "Unknown";
    }
}

GpioPinDirection toHidDir(MCP2221::GPIODirection d)
{
    if (d == MCP2221::GPIODirection::INPUT)
    {
        return GpioPinDirection::Input;
    }
    return GpioPinDirection::Output;
}

} // namespace

// cf. TABLE 1-5: GP DESIGNATION TABLE
const array<array<MCP2221::Assignment, 10>, 4> MCP2221::_pinMode2Assign{
    {
        //          GPIO,             ADC,                    DAC,                    LED_I2C,                    USBCFG,                     CLK OUT,                    SSPND,                      LED_URX,                LED_UTX,                IOC
        /* GP0 */ {{Assignment::GPIO, Assignment::UNKNOWN,    Assignment::UNKNOWN,    Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::DEDICATED_FUNC, Assignment::ALT_FUNC_0, Assignment::UNKNOWN,    Assignment::UNKNOWN}},
        /* GP1 */ {{Assignment::GPIO, Assignment::ALT_FUNC_0, Assignment::UNKNOWN,    Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::DEDICATED_FUNC, Assignment::UNKNOWN,        Assignment::UNKNOWN,    Assignment::ALT_FUNC_1, Assignment::ALT_FUNC_2}},
        /* GP2 */ {{Assignment::GPIO, Assignment::ALT_FUNC_0, Assignment::ALT_FUNC_1, Assignment::UNKNOWN,        Assignment::DEDICATED_FUNC, Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::UNKNOWN,    Assignment::UNKNOWN,    Assignment::UNKNOWN}},
        /* GP3 */ {{Assignment::GPIO, Assignment::ALT_FUNC_0, Assignment::ALT_FUNC_1, Assignment::DEDICATED_FUNC, Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::UNKNOWN,        Assignment::UNKNOWN,    Assignment::UNKNOWN,    Assignment::UNKNOWN}},
    }};

MCP2221::MCP2221(ApplicationServices& app, config::Node node) :
    MCP2221(defaultUsbId(), app.loggerPtr(node))
{
}

MCP2221::MCP2221(const protocol::usb::USB_ID& usbId, const shared_ptr<Logger>& logger) :
    I2CMaster(),
    _logger(logger),
    _dev(std::make_unique<mcp2221a::hid::Device>()),
    _vid(usbId._vid),
    _pid(usbId._pid),
    _pinModes({{0, 0, 0, 0}}),
    _pinDirections({{0, 0, 0, 0}}),
    _pinValues({{0, 0, 0, 0}}),
    _adcVref(0),
    _dacVref(0),
    _adcDatas({{0, 0, 0}}),
    _dacValue(0)
{
}

MCP2221::~MCP2221()
{
    close();
}

void MCP2221::hidFailure(const char* context)
{
    const Error e = _dev ? _dev->lastError() : Error::NotConnected;
    if (_dev && e == Error::UsbTransfer)
    {
        const char* lusb = _dev->lastLibusbErrorName();
        if (lusb != nullptr && lusb[0] != '\0')
        {
            logger().log(LogService::LogLevel::ERROR, "{} : mcp2221a_hid error {} ({}) {}", context, static_cast<int>(e), errorName(e), lusb);
        }
        else
        {
            logger().log(LogService::LogLevel::ERROR, "{} : mcp2221a_hid error {} ({}) libusb code {}", context, static_cast<int>(e), errorName(e), _dev->lastLibusbError());
        }
    }
    else
    {
        logger().log(LogService::LogLevel::ERROR, "{} : mcp2221a_hid error {} ({})", context, static_cast<int>(e), errorName(e));
    }
    close();
}

bool MCP2221::loadSramFromDevice()
{
    Report raw{};
    if (!_dev->getSramSettings(raw))
    {
        return false;
    }
    // Copy bytes 0-25; indices match Table 3-39 (GET SRAM response).
    std::copy_n(raw.begin(), _sramBytes.size(), _sramBytes.begin());
    _sramDirty = DirtyNone; // Fresh read, nothing modified yet
    return true;
}

bool MCP2221::syncSramFromDevice()
{
    if (!loadSramFromDevice())
    {
        hidFailure("getSramSettings");
        return false;
    }
    return true;
}

bool MCP2221::applySramSettings()
{
    // Build SET SRAM command payload (Table 3-36) from GET SRAM data (Table 3-39).
    // setSramSettings expects bytes 2-11 of the command (10 bytes total).
    // Bit 7 of each byte is the "alter" flag; only set if the corresponding dirty flag is active.
    //
    // SET payload idx:  SET byte:  Source from _sramBytes (GET byte):
    //   [0]             byte 2     Clock divider        <-- GET byte 4
    //   [1]             byte 3     DAC Vref             <-- GET byte 6 (bits 7:5)
    //   [2]             byte 4     DAC output value     <-- GET byte 6 (bits 4:0)
    //   [3]             byte 5     ADC Vref             <-- GET byte 8 (bits 7:5)
    //   [4]             byte 6     Interrupt            <-- GET byte 7
    //   [5]             byte 7     Alter GP designation (flag only)
    //   [6]             byte 8     GP0 settings         <-- GET byte 22
    //   [7]             byte 9     GP1 settings         <-- GET byte 23
    //   [8]             byte 10    GP2 settings         <-- GET byte 24
    //   [9]             byte 11    GP3 settings         <-- GET byte 25
    constexpr std::uint8_t AlterBit = 0x80U;
    const std::uint8_t alterClk     = (_sramDirty & DirtyClockDiv) ? AlterBit : 0U;
    const std::uint8_t alterDacVref = (_sramDirty & DirtyDacVref) ? AlterBit : 0U;
    const std::uint8_t alterDacVal  = (_sramDirty & DirtyDacValue) ? AlterBit : 0U;
    const std::uint8_t alterAdcVref = (_sramDirty & DirtyAdcVref) ? AlterBit : 0U;
    const std::uint8_t alterInt     = (_sramDirty & DirtyInterrupt) ? AlterBit : 0U;
    const std::uint8_t alterGp      = (_sramDirty & DirtyGpSettings) ? AlterBit : 0U;

    std::array<std::uint8_t, 10U> setPayload{};
    setPayload[0] = static_cast<std::uint8_t>(alterClk | (_sramBytes[getSramIndex::ClockDivider] & 0x7FU));
    setPayload[1] = static_cast<std::uint8_t>(alterDacVref | ((_sramBytes[getSramIndex::DacRefValue] >> 5) & 0x07U));
    setPayload[2] = static_cast<std::uint8_t>(alterDacVal | (_sramBytes[getSramIndex::DacRefValue] & 0x1FU));
    setPayload[3] = static_cast<std::uint8_t>(alterAdcVref | ((_sramBytes[getSramIndex::AdcRef] >> 5) & 0x07U));
    setPayload[4] = static_cast<std::uint8_t>(alterInt | (_sramBytes[getSramIndex::Interrupt] & 0x7FU));
    setPayload[5] = alterGp;
    setPayload[6] = _sramBytes[getSramIndex::Gp0];
    setPayload[7] = _sramBytes[getSramIndex::Gp1];
    setPayload[8] = _sramBytes[getSramIndex::Gp2];
    setPayload[9] = _sramBytes[getSramIndex::Gp3];

    if (!_dev->setSramSettings(setPayload))
    {
        hidFailure("setSramSettings");
        return false;
    }

    _sramDirty = DirtyNone; // Reset dirty flags after successful write
    return true;
}

int MCP2221::init()
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (_dev->isOpen())
    {
        logger().log(LogService::LogLevel::DEBUG, "MCP2221 init() (already open)");
        return 0;
    }

    const int n = _dev->countDevices(static_cast<std::uint16_t>(_vid), static_cast<std::uint16_t>(_pid));
    if (n <= 0)
    {
        logger().log(LogService::LogLevel::ERROR, "countDevices : no MCP2221A matching VID/PID (count={})", n);
        return -1;
    }

    if (!_dev->openByIndex(static_cast<std::uint16_t>(_vid), static_cast<std::uint16_t>(_pid), 0))
    {
        hidFailure("openByIndex");
        return -1;
    }

    // Sync HID pipe before SRAM read (avoids stale/partial IN after claim on Windows).
    (void)_dev->statusSetParameters(CancelI2cTransfer::NoEffect, SetI2cSpeedTag::NoEffect, I2cClockDivider{});

    constexpr int SramSyncRetries = 3;
    for (int attempt = 0; attempt < SramSyncRetries; ++attempt)
    {
        if (attempt > 0)
        {
            tools::os::thread::Thread::sleep_for(50_ms);
        }
        if (loadSramFromDevice())
        {
            logger().log(LogService::LogLevel::DEBUG, "MCP2221 init()");
            return 0;
        }
    }
    hidFailure("getSramSettings");
    return -1;
}

int MCP2221::setSpeed(I2CMaster::Speed speed)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const std::uint32_t kbs   = static_cast<std::uint32_t>(speed);
    const I2cClockDivider div = I2cClockDivider::fromNominalKbs(kbs);
    _i2cDividerByte           = div._value;

    int loop              = 0;
    constexpr int MaxLoop = 10;
    bool ok               = false;
    do
    {
        if (loop > 0)
        {
            (void)_dev->statusSetParameters(CancelI2cTransfer::Cancel, SetI2cSpeedTag::NoEffect, I2cClockDivider{});
            tools::os::thread::Thread::sleep_for(500_ms);
        }
        ok = _dev->statusSetParameters(CancelI2cTransfer::NoEffect, SetI2cSpeedTag::ApplyDivider, div);
        ++loop;
    } while (!ok && loop < MaxLoop);

    if (!ok)
    {
        hidFailure("statusSetParameters (I2C speed)");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "I2C setSpeed({})", static_cast<unsigned int>(speed));
    return 0;
}

int MCP2221::read(uint8_t addr, uint8_t* buf, size_t len)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }
    if (len > 0xFFFFU)
    {
        logger().log(LogService::LogLevel::ERROR, "I2C read length too large");
        return -1;
    }

    const auto r = I2cClientAddress::forRead7bit(addr);
    const I2cRetryPolicy policy =
        I2cRetryPolicy::forI2cRead(I2cClockDivider::fromRegisterValue(_i2cDividerByte), static_cast<std::uint16_t>(len));

    if (!_dev->i2cReadComplete(r, static_cast<std::uint16_t>(len), MutableByteSpan(buf, len), policy))
    {
        hidFailure("i2cReadComplete");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "I2C read(addr:0x{:02x}, len:0x{:04x})", static_cast<unsigned int>(addr), static_cast<unsigned int>(len));
    return static_cast<int>(len);
}

int MCP2221::write(uint8_t addr, const uint8_t* buf, size_t len)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }
    if (len > 0xFFFFU)
    {
        logger().log(LogService::LogLevel::ERROR, "I2C write length too large");
        return -1;
    }

    const auto w = I2cClientAddress::forWrite7bit(addr);
    if (!_dev->i2cWrite(w, ConstByteSpan(buf, len)))
    {
        hidFailure("i2cWrite");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "I2C write(addr:0x{:02x}, len:0x{:04x})", static_cast<unsigned int>(addr), static_cast<unsigned int>(len));
    return static_cast<int>(len);
}

int MCP2221::readByte(uint8_t addr, uint8_t cmd, uint8_t& value)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const std::array<std::uint8_t, 1> wdata{cmd};
    std::array<std::uint8_t, 1> out{};
    std::size_t nOut = 0U;
    const auto w     = I2cClientAddress::forWrite7bit(addr);
    const auto r     = I2cClientAddress::forRead7bit(addr);

    if (!_dev->i2cWriteThenRead(w, ConstByteSpan(wdata), r, 1U, MutableByteSpan(out), nOut, I2cClockDivider::fromRegisterValue(_i2cDividerByte)))
    {
        hidFailure("i2cWriteThenRead (SMBus readByte)");
        return -1;
    }
    if (nOut != 1U)
    {
        logger().log(LogService::LogLevel::ERROR, "SMBus readByte : short read");
        return -1;
    }
    value = out[0];

    logger().log(LogService::LogLevel::DEBUG, "SMB readByte(addr:0x{0:02x}, cmd:0x{1:02x}, value:0x{2:02x}|{2:08b})", static_cast<unsigned int>(addr), static_cast<unsigned int>(cmd), static_cast<unsigned int>(value));
    return 0;
}

int MCP2221::readWord(uint8_t addr, uint8_t cmd, uint16_t& value)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const std::array<std::uint8_t, 1> wdata{cmd};
    std::array<std::uint8_t, 2> out{};
    std::size_t nOut = 0U;
    const auto w     = I2cClientAddress::forWrite7bit(addr);
    const auto r     = I2cClientAddress::forRead7bit(addr);

    if (!_dev->i2cWriteThenRead(w, ConstByteSpan(wdata), r, 2U, MutableByteSpan(out), nOut, I2cClockDivider::fromRegisterValue(_i2cDividerByte)))
    {
        hidFailure("i2cWriteThenRead (SMBus readWord)");
        return -1;
    }
    if (nOut != 2U)
    {
        logger().log(LogService::LogLevel::ERROR, "SMBus readWord : short read");
        return -1;
    }
    value = static_cast<std::uint16_t>(static_cast<std::uint16_t>(out[1]) << 8 | out[0]);

    logger().log(LogService::LogLevel::DEBUG, "SMB  readWord(addr:0x{0:02x}, cmd:0x{1:02x}, value:0x{2:04x}|{2:016b})", static_cast<unsigned int>(addr), static_cast<unsigned int>(cmd), static_cast<unsigned int>(value));
    return 0;
}

int MCP2221::readBlock(uint8_t addr, uint8_t cmd, std::vector<uint8_t>& values)
{
    (void)addr;
    (void)cmd;
    (void)values;
    logger().log(LogService::LogLevel::ERROR, "Not yet implemented");
    return -1;
}

int MCP2221::writeByte(uint8_t addr, uint8_t cmd, uint8_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const std::array<std::uint8_t, 2> payload{cmd, value};
    const auto w = I2cClientAddress::forWrite7bit(addr);
    if (!_dev->i2cWrite(w, ConstByteSpan(payload)))
    {
        hidFailure("i2cWrite (SMBus writeByte)");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "SMB writeByte(addr:0x{0:02x}, cmd:0x{1:02x}, value:0x{2:02x}|{2:08b})", static_cast<unsigned int>(addr), static_cast<unsigned int>(cmd), static_cast<unsigned int>(value));
    return 0;
}

int MCP2221::writeWord(uint8_t addr, uint8_t cmd, uint16_t value)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const std::array<std::uint8_t, 3> payload{cmd, static_cast<std::uint8_t>(value & 0xFFU), static_cast<std::uint8_t>((value >> 8) & 0xFFU)};
    const auto w = I2cClientAddress::forWrite7bit(addr);
    if (!_dev->i2cWrite(w, ConstByteSpan(payload)))
    {
        hidFailure("i2cWrite (SMBus writeWord)");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "SMB writeWord(addr:0x{0:02x}, cmd:0x{1:02x}, value:0x{2:04x}|{2:016b})", static_cast<unsigned int>(addr), static_cast<unsigned int>(cmd), static_cast<unsigned int>(value));
    return 0;
}

int MCP2221::writeBlock(uint8_t addr, uint8_t cmd, const std::vector<uint8_t>& values)
{
    (void)addr;
    (void)cmd;
    (void)values;
    logger().log(LogService::LogLevel::ERROR, "Not yet implemented");
    return -1;
}

int MCP2221::setGPPinMode(GPPin pin, GPMode mode)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    const Assignment assignment = _pinMode2Assign[static_cast<int>(pin)][static_cast<int>(mode)];
    if (assignment == Assignment::UNKNOWN)
    {
        logger().log(LogService::LogLevel::ERROR, "setGPPinMode : Error(GPPin/GPCode Unknown)");
        return -1;
    }

    if (!syncSramFromDevice())
    {
        return -1;
    }

    // Table 3-39: GP0-GP3 are at indices 22-25.
    const std::size_t idx     = getSramIndex::Gp0 + static_cast<std::size_t>(pin);
    const std::uint8_t newVal = static_cast<std::uint8_t>(
        (_sramBytes[idx] & 0xF8U) | (static_cast<std::uint8_t>(assignment) & 0x07U));
    if (_sramBytes[idx] != newVal)
    {
        _sramBytes[idx] = newVal;
        _sramDirty |= DirtyGpSettings;
    }
    _pinModes[static_cast<std::size_t>(pin)] = static_cast<std::uint8_t>(assignment) & 0x07U;

    if (_sramDirty == DirtyNone)
    {
        return 0; // Nothing changed
    }
    if (!applySramSettings())
    {
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "GPIO setGPPinMode(pin:{}, mode:{})", static_cast<unsigned int>(pin), static_cast<unsigned int>(mode));
    return 0;
}

int MCP2221::setGPIODirection(GPPin pin, GPIODirection dir)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    std::array<GpioPinCommandBlock, 4U> pins{};
    for (auto& b : pins)
    {
        b._alterOutput    = GpioAlterField::NoChange;
        b._alterDirection = GpioAlterField::NoChange;
    }
    const std::size_t i     = static_cast<std::size_t>(pin);
    pins[i]._alterDirection = GpioAlterField::Apply;
    pins[i]._direction      = toHidDir(dir);
    _pinDirections[i]       = static_cast<std::uint8_t>(dir);

    if (!_dev->setGpioOutputs(pins))
    {
        hidFailure("setGpioOutputs (direction)");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "GPIO setGPIODirection(pin:{}, dir:0x{:02x})", static_cast<unsigned int>(pin), static_cast<unsigned int>(dir));
    return 0;
}

int MCP2221::setGPIOState(GPPin pin, GPIOState state)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }
    if ((state != GPIOState::LOW) && (state != GPIOState::HIGH))
    {
        return -1;
    }

    std::array<GpioPinCommandBlock, 4U> pins{};
    for (auto& b : pins)
    {
        b._alterOutput    = GpioAlterField::NoChange;
        b._alterDirection = GpioAlterField::NoChange;
    }
    const std::size_t i  = static_cast<std::size_t>(pin);
    pins[i]._alterOutput = GpioAlterField::Apply;
    pins[i]._outputValue = (state == GPIOState::HIGH) ? GpioOutputLevel::LogicHigh : GpioOutputLevel::LogicLow;
    _pinValues[i]        = static_cast<std::uint8_t>(state);

    if (!_dev->setGpioOutputs(pins))
    {
        hidFailure("setGpioOutputs (value)");
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "GPIO setGPIOState(pin:{}, state:0x{:02x})", static_cast<unsigned int>(pin), static_cast<unsigned int>(state));
    return 0;
}

int MCP2221::getGPIOState(GPPin pin, GPIOState& state)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    std::array<GpioPinState, 4U> vals{};
    if (!_dev->getGpioValues(vals))
    {
        hidFailure("getGpioValues");
        return -1;
    }

    const std::size_t i = static_cast<std::size_t>(pin);
    state               = (vals[i]._level == GpioOutputLevel::LogicHigh) ? GPIOState::HIGH : GPIOState::LOW;

    logger().log(LogService::LogLevel::DEBUG, "GPIO getGPIOState(pin:{}, state:0x{:02x}", static_cast<unsigned int>(pin), static_cast<unsigned int>(state));
    return 0;
}

int MCP2221::setADCVRef(GPVRef vref)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    _adcVref = static_cast<std::uint8_t>(vref);
    if (!syncSramFromDevice())
    {
        return -1;
    }
    // Table 3-39: ADC Vref is in byte 8, bits 7:5 (option + sel).
    const std::uint8_t newVal = static_cast<std::uint8_t>(
        (_sramBytes[getSramIndex::AdcRef] & 0x1FU) | ((_adcVref & 0x07U) << 5));
    if (_sramBytes[getSramIndex::AdcRef] != newVal)
    {
        _sramBytes[getSramIndex::AdcRef] = newVal;
        _sramDirty |= DirtyAdcVref;
    }
    if (_sramDirty == DirtyNone)
    {
        return 0;
    }
    if (!applySramSettings())
    {
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "ADC setADCVRef(vref:{})", static_cast<unsigned int>(vref));
    return 0;
}

int MCP2221::getADCData(GPPin pin, unsigned int& adcData)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    StatusSnapshot snap{};
    if (!_dev->getStatusSnapshot(snap))
    {
        hidFailure("getStatusSnapshot");
        return -1;
    }
    for (std::size_t k = 0U; k < 3U; ++k)
    {
        _adcDatas[k] = snap._adcCounts[k];
    }
    adcData = _adcDatas[static_cast<int>(pin) - 1];

    logger().log(LogService::LogLevel::DEBUG, "ADC getADCData(adcData:0x{:04x})", adcData);
    return 0;
}

int MCP2221::setDACVref(GPVRef vref)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }

    _dacVref = static_cast<std::uint8_t>(vref);
    if (!syncSramFromDevice())
    {
        return -1;
    }
    // Table 3-39: DAC Vref is in byte 6, bits 7:5 (option + sel). Bits 4:0 = DAC value.
    const std::uint8_t newVal = static_cast<std::uint8_t>(
        (_sramBytes[getSramIndex::DacRefValue] & 0x1FU) | ((_dacVref & 0x07U) << 5));
    if (_sramBytes[getSramIndex::DacRefValue] != newVal)
    {
        _sramBytes[getSramIndex::DacRefValue] = newVal;
        _sramDirty |= DirtyDacVref;
    }
    if (_sramDirty == DirtyNone)
    {
        return 0;
    }
    if (!applySramSettings())
    {
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "DAC setDACVref(vref:{})", static_cast<unsigned int>(vref));
    return 0;
}

int MCP2221::setDACValue(unsigned int value)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev->isOpen())
    {
        logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
        return -1;
    }
    if (value > 31U)
    {
        logger().log(LogService::LogLevel::ERROR, "Valid range is between 0 and 31");
        return -1;
    }

    _dacValue = static_cast<std::uint8_t>(value);
    if (!syncSramFromDevice())
    {
        return -1;
    }
    // Table 3-39: DAC output value is in byte 6, bits 4:0 (5-bit value). Bits 7:5 = Vref.
    const std::uint8_t newVal = static_cast<std::uint8_t>(
        (_sramBytes[getSramIndex::DacRefValue] & 0xE0U) | (_dacValue & 0x1FU));
    if (_sramBytes[getSramIndex::DacRefValue] != newVal)
    {
        _sramBytes[getSramIndex::DacRefValue] = newVal;
        _sramDirty |= DirtyDacValue;
    }
    if (_sramDirty == DirtyNone)
    {
        return 0;
    }
    if (!applySramSettings())
    {
        return -1;
    }

    logger().log(LogService::LogLevel::DEBUG, "DAC setDACValue(value:{})", value);
    return 0;
}

void MCP2221::close()
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (!_dev || !_dev->isOpen())
    {
        return;
    }

    _dev->close();
    logger().log(LogService::LogLevel::DEBUG, "MCP2221 close()");
}

int MCP2221::busCycleBegin()
{
    {
        const std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!_dev->isOpen())
        {
            logger().log(LogService::LogLevel::ERROR, "Need to be initialized before use");
            return -1;
        }

        StatusSnapshot snap{};
        if (!_dev->getStatusSnapshot(snap))
        {
            hidFailure("getStatusSnapshot (busCycleBegin)");
            return -1;
        }
        for (std::size_t k = 0U; k < 3U; ++k)
        {
            _adcDatas[k] = snap._adcCounts[k];
        }
    }

    update(_adcDatas);
    return 0;
}

FOUNDATION_FACTORY_REGISTER(driver::chip::MCP2221,
                            "driver::chip::MCP2221",
                            driver_chip_MCP2221)
