/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Serport.h
 * @brief Serial port driver using the POSIX API.
 */
#pragma once

#include "tools/design/factory/IObject.hpp"
#include "tools/os/serport/ISerport.h"

#include <chrono>
#include <mutex>
#include <string>
#include <termios.h>

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

    [[nodiscard]] int read(char* buffer, unsigned int size) override;
    [[nodiscard]] int read(char* buffer, unsigned int size, util::chrono::Delay delay) override;
    [[nodiscard]] int write(const char* buffer, unsigned int size) override;
    [[nodiscard]] int getNRead() override;
    [[nodiscard]] int getNWrite() override { return -1; }
    int flush() override;
    int wflush() override;
    int rflush() override;
    int cancel() override { return -1; } ///< Not supported
    int setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop) override;
    int setFlowCtrl(FlowCtrl flowCtrl) override;
    [[nodiscard]] bool isReady() const override { return _bPortReady; }
    int reset() override;

protected:
    int openPort();
    void closePort();
    int configurePort();

    explicit Serport(const std::string& deviceName);

private:
    std::string _portName;       ///< Device name
    int _fd;                     ///< File descriptor
    bool _bPortReady;            ///< Port state
    std::recursive_mutex _mutex; // Protection against concurrent opening/parameterization
    std::mutex _mutexRead;       // Protection against concurrent reads
    std::mutex _mutexWrite;      // Protection against concurrent writes

    struct termios _tio;    ///< Current port settings
    struct termios _oldTio; ///< Previous port settings
};

} // namespace tools::os::serport
