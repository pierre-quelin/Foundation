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
    enum class FlowCtrl
    {
        FLOW_CTRL_NONE,
        FLOW_CTRL_XON_XOFF,
        FLOW_CTRL_HW
    };

protected:
    FlowCtrl _flowctrl;

public:
    [[nodiscard]] FlowCtrl getFlowCtrl() const { return _flowctrl; }

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
    [[nodiscard]] virtual bool isReady() const                                                 = 0;
    virtual int reset()                                                                        = 0;
};

} // namespace tools::os::serport
