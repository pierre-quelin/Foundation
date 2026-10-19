/**
 * Copyright (c) 2021–2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ISerport.h
 * @brief Serial port interface
 */
#pragma once

#include "util/chrono/Delay.hpp"

#include <chrono>

namespace tools::os::serport
{

/**
 * @brief Mode for one modem signal pair (RTS↔CTS or DTR↔DSR).
 *
 * - Off: force local output off; ignore remote input (no TX gating).
 * - Assert: force local output on; ignore remote input (e.g. CSC RTS).
 * - FlowControl: OS-driven handshake both ways (output auto + gate TX on input).
 */
enum class LinePairMode
{
    Off,
    Assert,
    FlowControl
};

struct ModemCtrl
{
    LinePairMode rtsCts{LinePairMode::Off};
    LinePairMode dtrDsr{LinePairMode::Off};
};

/** @brief Live levels of the four modem lines (true = asserted / active). */
struct ModemStatus
{
    bool rts{false};
    bool cts{false};
    bool dtr{false};
    bool dsr{false};
};

class ISerport
{
public:
    enum class BitRate
    {
        BITRATE_1200   = 1200,
        BITRATE_2400   = 2400,
        BITRATE_4800   = 4800,
        BITRATE_9600   = 9600,
        BITRATE_19200  = 19200,
        BITRATE_38400  = 38400,
        BITRATE_57600  = 57600,
        BITRATE_115200 = 115200
    };

protected:
    BitRate _bitrate;

public:
    [[nodiscard]] BitRate getBitrate() const { return _bitrate; }
    enum class Parity
    {
        PARITY_NONE,
        PARITY_ODD,
        PARITY_EVEN,
        PARITY_MARK,
        PARITY_SPACE
    };

protected:
    Parity _parity;

public:
    [[nodiscard]] Parity getParity() const { return _parity; }
    enum class DataBit
    {
        DATABIT_4 = 4,
        DATABIT_5 = 5,
        DATABIT_6 = 6,
        DATABIT_7 = 7,
        DATABIT_8 = 8
    };

protected:
    DataBit _databit;

public:
    [[nodiscard]] DataBit getDataBit() const { return _databit; }
    enum class StopBit
    {
        STOPBIT_1  = 1,
        STOPBIT_2  = 2,
        STOPBIT_15 = 3
    };

protected:
    StopBit _stopBit;

public:
    [[nodiscard]] StopBit getStopBit() const { return _stopBit; }

    /**
     * @brief Software / legacy flow control.
     * @note Hardware RTS/CTS (and DTR/DSR) is configured via @ref setModemCtrl.
     *       @c FLOW_CTRL_HW is kept as an alias that sets @c rtsCts to FlowControl.
     */
    enum class FlowCtrl
    {
        FLOW_CTRL_NONE,
        FLOW_CTRL_XON_XOFF,
        FLOW_CTRL_HW ///< deprecated alias → ModemCtrl.rtsCts = FlowControl
    };

protected:
    FlowCtrl _flowctrl;
    ModemCtrl _modemCtrl{};

public:
    [[nodiscard]] FlowCtrl getFlowCtrl() const { return _flowctrl; }
    [[nodiscard]] ModemCtrl getModemCtrl() const { return _modemCtrl; }

public:
    virtual ~ISerport() = default;
    ISerport() :
        _bitrate(BitRate::BITRATE_9600),
        _parity(Parity::PARITY_EVEN),
        _databit(DataBit::DATABIT_8),
        _stopBit(StopBit::STOPBIT_1),
        _flowctrl(FlowCtrl::FLOW_CTRL_NONE) {}
    ISerport(const ISerport&)            = delete;
    ISerport& operator=(const ISerport&) = delete;
    ISerport(ISerport&&)                 = delete;
    ISerport& operator=(ISerport&&)      = delete;

    [[nodiscard]] virtual int read(char* buffer, unsigned int size)                            = 0;
    [[nodiscard]] virtual int read(char* buffer, unsigned int size, util::chrono::Delay delay) = 0;
    [[nodiscard]] virtual int write(const char* buffer, unsigned int size)                     = 0;
    [[nodiscard]] virtual int getNRead()                                                       = 0;
    [[nodiscard]] virtual int getNWrite()                                                      = 0;
    virtual int flush()                                                                        = 0;
    virtual int wflush()                                                                       = 0;
    virtual int rflush()                                                                       = 0;
    virtual int cancel()                                                                       = 0;
    virtual int setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop)          = 0;
    virtual int setFlowCtrl(FlowCtrl flowCtrl)                                                 = 0;
    virtual int setModemCtrl(const ModemCtrl& modem)                                           = 0;
    [[nodiscard]] virtual ModemStatus getModemStatus() const                                   = 0;
    [[nodiscard]] virtual bool isReady() const                                                 = 0;
    virtual int reset()                                                                        = 0;
};

} // namespace tools::os::serport
