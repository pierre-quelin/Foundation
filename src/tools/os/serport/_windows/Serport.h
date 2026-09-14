/**
 * Copyright (c) 2021–2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Serport.h
 * @brief Serial port driver using the serial ports of a Windows PC.
 */
#pragma once

// windows.h before ISerport.h so PARITY_* #undef in ISerport.h takes effect.
// clang-format off
#include <windows.h> // HANDLE, DCB, COMMTIMEOUTS

#if defined(WIN32)
// Prevent Windows namespace pollution
// Undefine macros defined in Windows headers that conflict with ISerport::Parity
#undef PARITY_NONE
#undef PARITY_ODD
#undef PARITY_EVEN
#undef PARITY_MARK
#undef PARITY_SPACE
#endif

#include "tools/os/serport/ISerport.h"
#include "tools/design/factory/IObject.hpp"
// clang-format on

#include <chrono>
#include <mutex>
#include <string>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace tools::os::serport
{

class Serport : public tools::design::factory::IObject, public ISerport
{
public:
    virtual ~Serport();
    Serport(tools::design::ApplicationServices& app, tools::design::config::Node node);
    Serport(const Serport&)            = delete;
    Serport& operator=(const Serport&) = delete;
    Serport(Serport&&)                 = delete;
    Serport& operator=(Serport&&)      = delete;

    [[nodiscard]] int read(char* buffer, unsigned int size) override; ///< Does not work on Windows
    [[nodiscard]] int read(char* buffer, unsigned int size, util::chrono::Delay delay) override;
    [[nodiscard]] int write(const char* buffer, unsigned int size) override;
    [[nodiscard]] int getNRead() override;
    [[nodiscard]] int getNWrite() override;
    int flush() override;
    int wflush() override;
    int rflush() override;
    int cancel() override;
    int setParams(BitRate bRate, DataBit nData,
                  Parity parity, StopBit nStop) override;
    int setFlowCtrl(FlowCtrl flowCtrl) override;
    [[nodiscard]] bool isReady() const override { return _bPortReady; }
    int reset() override;

protected:
    int openPort(const std::string& portname);
    void closePort();
    int configurePort();
    int internalRead(char* buffer, unsigned int size, unsigned long tout);
    void changePortReady(bool ready);

    explicit Serport(const std::string& deviceName);

private:
    std::string _portName;      ///< Device name
    bool _bPortReady;           ///< Port state
    HANDLE _hComm;              ///< Port handle
    DCB _dcb;                   ///< Port configuration
    COMMTIMEOUTS _commTimeouts; ///< Port time-outs configuration

    std::recursive_mutex _mutex;      // Protection against concurrent opening/parameterization
    std::mutex _mutexRead;  // Protection against concurrent reads
    std::mutex _mutexWrite; // Protection against concurrent writes

    // Windows events for asynchronous I/O and cancellation management
    HANDLE _readEvent;
    HANDLE _writeEvent;
    bool _canceledRead;
    bool _canceledWrite;
};

} // namespace tools::os::serport
