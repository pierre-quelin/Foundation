/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/serport/Serport.h"

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Register.hpp"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;
using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::factory;
using namespace tools::os::serport;

// Prefixes for buffer traces
static const std::string TxPrefix("S ");
static const std::string RxPrefix("R ");

/**
 * Constructor
 */
Serport::Serport(const std::string& deviceName) :
    _portName(deviceName),
    _fd(-1),
    _bPortReady(false)
{
    BitRate bitRate   = BitRate::BITRATE_9600;
    Parity parity     = Parity::PARITY_NONE;
    DataBit dataBit   = DataBit::DATABIT_8;
    StopBit stopBit   = StopBit::STOPBIT_1;
    FlowCtrl flowCtrl = FlowCtrl::FLOW_CTRL_NONE;

    if (0 != openPort())
    {
        // We tolerate a start-up without being able to open the port
        // -> we leave ready to false and the user can use reset() to retry to open the port
        //        logErr( "tools::os::Serport - open [%s] ERROR %d", _portName, errno );
        return;
    }

    // Disable canonical input (not set = raw input)
    // Disable echoing of input characters
    // Do not echo erase character as BS-SP-BS
    _tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    // Local connection, no modem control
    // Enable receiving characters
    _tio.c_cflag |= (CLOCAL | CREAD);
    // No postprocess output (not set = raw output)
    _tio.c_oflag &= ~OPOST;
    // The minimum number of characters to read
    _tio.c_cc[VMIN] = 0; // 0 char min but reset at each read()
    // The time to wait for the first character read (tenths of seconds)
    _tio.c_cc[VTIME] = 0; // No timeout but reset at each read()

    // Make changes now without waiting for data to complete
    if (0 != configurePort())
    {
        //        logErr( "tools::os::Serport - Configure device" );
        return;
    }

    if ((0 != setParams(bitRate, dataBit, parity, stopBit)) ||
        (0 != setFlowCtrl(flowCtrl)))
    {
        //        logErr( "tools::os::Serport - Configure device" );
        return;
    }

    // Flush read and write buffers
    flush();

    _bPortReady = true;
}

/**
 * Destructor
 */
Serport::~Serport()
{
    // Restore old port settings and close the serial port
    closePort();
}

int Serport::setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    int returnValue = 0;

    // Baud rate
    switch (bRate)
    {
        case BitRate::BITRATE_1200:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B1200;
            break;
        case BitRate::BITRATE_2400:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B2400;
            break;
        case BitRate::BITRATE_4800:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B4800;
            break;
        case BitRate::BITRATE_9600:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B9600;
            break;
        case BitRate::BITRATE_19200:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B19200;
            break;
        case BitRate::BITRATE_38400:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B38400;
            break;
        case BitRate::BITRATE_57600:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B57600;
            break;
        case BitRate::BITRATE_115200:
            _tio.c_cflag &= ~CBAUD;
            _tio.c_cflag |= B115200;
            break;
        default:
            returnValue = -1;
            break;
    }

    // Data bits per byte
    switch (nData)
    {
        case DataBit::DATABIT_5:
            _tio.c_cflag &= ~CSIZE;
            _tio.c_cflag |= CS5;
            break;
        case DataBit::DATABIT_6:
            _tio.c_cflag &= ~CSIZE;
            _tio.c_cflag |= CS6;
            break;
        case DataBit::DATABIT_7:
            _tio.c_cflag &= ~CSIZE;
            _tio.c_cflag |= CS7;
            break;
        case DataBit::DATABIT_8:
            _tio.c_cflag &= ~CSIZE;
            _tio.c_cflag |= CS8;
            break;
        default:
            returnValue = -1;
            break;
    }

    // Parity
    switch (parity)
    {
        case Parity::PARITY_NONE:
            _tio.c_cflag &= ~PARENB;
            break;
        case Parity::PARITY_ODD:
            _tio.c_cflag |= PARENB;
            _tio.c_cflag |= PARODD;
            break;
        case Parity::PARITY_EVEN:
            _tio.c_cflag |= PARENB;
            _tio.c_cflag &= ~PARODD;
            break;
        case Parity::PARITY_MARK:
        case Parity::PARITY_SPACE:
        default:
            returnValue = -1;
            break;
    }

    // Stop bits
    switch (nStop)
    {
        case StopBit::STOPBIT_1:
            _tio.c_cflag &= ~CSTOPB;
            break;
        case StopBit::STOPBIT_15:
            returnValue = -1;
            break;
        case StopBit::STOPBIT_2:
            _tio.c_cflag |= CSTOPB;
            break;
        default:
            returnValue = -1;
            break;
    }

    if (0 != returnValue)
    {
        return -1;
    }

    return configurePort();
}

/**
 * Set flow control parameters
 */
int Serport::setFlowCtrl(FlowCtrl flowCtrl)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    // Flow control settings
    switch (flowCtrl)
    {
        case FlowCtrl::FLOW_CTRL_NONE:
            _tio.c_iflag &= ~(IXON | IXOFF | IXANY);
            _tio.c_cflag &= ~CRTSCTS;
            break;
        case FlowCtrl::FLOW_CTRL_XON_XOFF:
            _tio.c_iflag |= (IXON | IXOFF | IXANY);
            _tio.c_cflag &= ~CRTSCTS;
            break;
        case FlowCtrl::FLOW_CTRL_HW:
            _tio.c_iflag &= ~(IXON | IXOFF | IXANY);
            _tio.c_cflag |= CRTSCTS;
            break;
        default:
            return -1;
    }

    return configurePort();
}

/**
 * Writes up to size bytes to the serial port.
 *
 * @param[in] buffer the buffer to write.
 * @param[in] size the number of bytes to write.
 * @return the number of bytes written, or -1 on error.
 */
int Serport::write(const char* buffer, unsigned int size)
{
    const std::lock_guard<std::mutex> lock(_mutexWrite);

    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        if (!_bPortReady && (0 != reset()))
        {
            return -1;
        }
    }

    int nbBytes = static_cast<int>(::write(_fd, buffer, static_cast<size_t>(size)));

    if (nbBytes > 0)
    {
        //        logInfo(TxPrefix, buffer, nbBytes);
    }
    else if (nbBytes < 0)
    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        _bPortReady = false;
        //        logErr( "Serport::write ERROR %d (size: %u)", errno, size );
    }

    return nbBytes;
}

/**
 * Attempts to read up to size bytes from the serial port (blocking).
 *
 * @param[out] buffer the read buffer.
 * @param[in] size number of bytes to read.
 * @return On success, the number of bytes read is returned. It is not an error
 * if this number is smaller than the number of bytes requested.
 */
int Serport::read(char* buffer, unsigned int size)
{
    const std::lock_guard<std::mutex> lock(_mutexRead);

    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        if (!_bPortReady && (0 != reset()))
        {
            return -1;
        }

        if ((0 != _tio.c_cc[VTIME]) || (1 != _tio.c_cc[VMIN]))
        {
            _tio.c_cc[VTIME] = 0; // Wait indefinitely for the first character
            _tio.c_cc[VMIN]  = 1; // Read at least one character

            // Make changes now without waiting for data to complete
            if (0 != configurePort())
            {
                _bPortReady = false;
                return -1;
            }
        }
    }

    int nbBytes = static_cast<int>(::read(_fd, buffer, static_cast<size_t>(size)));

    if (nbBytes > 0)
    {
        //        logInfo(RxPrefix, buffer, nbBytes);
    }
    else if (nbBytes < 0)
    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        _bPortReady = false;
        //        logErr( "Serport::read ERROR %d", errno );
    }

    return nbBytes;
}

/**
 * Attempts to read up to size bytes from the serial port (with a timeout).
 *
 * @param[out] buffer the read buffer.
 * @param[in] size number of bytes to read.
 * @param[in] delay a timeout (100ms min).
 * @return On success, the number of bytes read is returned. It is not an error
 * if this number is smaller than the number of bytes requested.
 */
int Serport::read(char* buffer, unsigned int size, util::chrono::Delay delay)
{
    const std::lock_guard<std::mutex> lock(_mutexRead);

    const auto delayMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(delay.toNanoseconds());
    int timeoutTenths = static_cast<int>(delayMs.count() / 100);

    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        if (!_bPortReady && (0 != reset()))
        {
            return -1;
        }

        if ((timeoutTenths != _tio.c_cc[VTIME]) || (0 != _tio.c_cc[VMIN]))
        {
            _tio.c_cc[VTIME] = timeoutTenths; // Time to wait for the first character (tenths of seconds)
            _tio.c_cc[VMIN]  = 0;             // No minimum number of characters

            // Make changes now without waiting for data to complete
            if (0 != configurePort())
            {
                _bPortReady = false;
                return -1;
            }
        }
    }

    int nbBytes = static_cast<int>(::read(_fd, buffer, static_cast<size_t>(size)));

    if (nbBytes > 0)
    {
        //        logInfo(RxPrefix, buffer, nbBytes);
    }
    else if (nbBytes < 0)
    {
        const std::lock_guard<std::recursive_mutex> lockParams(_mutex);
        _bPortReady = false;
        //        logErr( "Serport::read ERROR %d", errno );
    }

    return nbBytes;
}

/**
 * @brief Reset the port
 */
int Serport::reset()
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    closePort();

    if (0 != openPort())
    {
        //        logInfo( "reset - openPort" );
        return -1;
    }

    if (0 != configurePort())
    {
        //        logInfo( "reset - configurePort" );
        return -1;
    }

    _bPortReady = true;
    return 0;
}

/**
 * @brief Obtain the number of chars in the read buffer
 */
int Serport::getNRead()
{
    int nbBytesUnread = 0;
    (void)ioctl(_fd, FIONREAD, &nbBytesUnread);
    return nbBytesUnread;
}

/**
 * @brief Clear the read and write buffers
 */
int Serport::flush()
{
    if (0 != tcflush(_fd, TCIOFLUSH))
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Clear the write buffer
 */
int Serport::wflush()
{
    if (0 != tcflush(_fd, TCOFLUSH))
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Clear the read buffer
 */
int Serport::rflush()
{
    if (0 != tcflush(_fd, TCIFLUSH))
    {
        return -1;
    }
    return 0;
}

int Serport::openPort()
{
    // Open the device in non-blocking mode
    _fd = open(_portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (_fd < 0)
    {
        //        logInfo( "tools::os::openPort - open [%s] ERROR %d", _portName, errno );
        return -1;
    }

    /**
     * cf. Serial-Programming-HOWTO.txt
     *
     * 3.  Opening
     *
     * "Normally, a serial device opens in "blocking" mode.  This means that
     * the open() will not return until the Carrier Detect line from the port
     * is active, e.g. modem, is active.  When opened with the O_NONBLOCK
     * flag set, the open() will return immediately regardless of the status
     * of the DCD line.  The "blocking" mode also affects the read() call.
     *
     * The fcntl(2) command can be used to change the O_NONBLOCK flag anytime
     * after the device has been opened."
     */
    // Restore blocking mode
    fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL) & ~O_NONBLOCK);

    // Save current port settings
    tcgetattr(_fd, &_oldTio);

    // Clear current port settings
    bzero(&_tio, sizeof(_tio));

    return 0;
}

int Serport::configurePort()
{
    // Make changes now without waiting for data to complete
    if (0 != tcsetattr(_fd, TCSANOW, &_tio))
    {
        //        logError( "tools::os::configurePort - tcsetattr ERROR %d", errno );
        return -1;
    }
    return 0;
}

void Serport::closePort()
{
    if (_fd >= 0)
    {
        tcsetattr(_fd, TCSANOW, &_oldTio);
        close(_fd);
        _fd = -1;
    }
}

Serport::Serport(ApplicationServices& app, Node node) :
    Serport(node.at("DeviceName").value<std::string>())
{
    (void)app;
}

FOUNDATION_FACTORY_REGISTER(tools::os::serport::Serport,
                            "tools::os::serport::Serport",
                            tools_os_serport_Serport)
