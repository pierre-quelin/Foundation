/**
 * Copyright (c) 2021–2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
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
Serport::Serport(const std::string& portname) :
        _portName(portname),
        _bPortReady(false), _hComm(NULL), _readEvent(NULL), _writeEvent(NULL), _canceledRead(false), _canceledWrite(false)
{
    string cfgParamName;
    BitRate bitRate   = Serport::BitRate::BITRATE_9600;
    Parity parity     = Serport::Parity::PARITY_NONE;
    DataBit dataBit   = Serport::DataBit::DATABIT_8;
    StopBit stopBit   = Serport::StopBit::STOPBIT_1;
    FlowCtrl flowCtrl = Serport::FlowCtrl::FLOW_CTRL_NONE;

    // Default settings
    _commTimeouts.ReadIntervalTimeout         = 0;
    _commTimeouts.ReadTotalTimeoutMultiplier  = 0;
    _commTimeouts.ReadTotalTimeoutConstant    = 0;
    _commTimeouts.WriteTotalTimeoutMultiplier = 0;
    _commTimeouts.WriteTotalTimeoutConstant   = 0;
    _dcb.DCBlength                            = sizeof(_dcb);
    // _dcb.BaudRate     initialized by setParams
    _dcb.fBinary = TRUE;
    // _dcb.fParity      initialized by setParams
    _dcb.fOutxCtsFlow    = FALSE;
    _dcb.fOutxDsrFlow    = FALSE;
    _dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    _dcb.fDsrSensitivity = FALSE;
    // _dcb.fOutX        initialized by setFlowCtrl
    // _dcb.fInX         initialized by setFlowCtrl
    _dcb.fErrorChar    = FALSE;
    _dcb.fNull         = FALSE;
    _dcb.fRtsControl   = RTS_CONTROL_DISABLE;
    _dcb.fAbortOnError = FALSE;
    _dcb.fDummy2       = 0;
    _dcb.wReserved     = 0;
    _dcb.XonLim        = 10;  // values to be specialized according to the application
    _dcb.XoffLim       = 100; //
    // _dcb.ByteSize     initialized by setParams
    // _dcb.Parity       initialized by setParams
    // _dcb.StopBits     initialized by setParams
    _dcb.XonChar    = 0x11;
    _dcb.XoffChar   = 0x13;
    _dcb.ErrorChar  = 0;
    _dcb.EofChar    = 0;
    _dcb.EvtChar    = 0;
    _dcb.wReserved1 = 0;

    // Opening the device with the default parameters
    bool ready = false;
    if (0 != openPort("\\\\.\\" + _portName))
    {
        // We tolerate a start-up without being able to open the port
        // -> we leave ready to false and the user can use reset() to retry to open the port
//        logErr( "tools::os::Serport - open [%s] ERROR %08x ", _portName, errno );
    }
    else
    {
        // Read initial port parameters
        if (!GetCommState(_hComm, &_dcb))
        {
//            logErr( "tools::os::Serport - GetCommState Error." );
            return;
        }

        // Configure the port with default parameters
        if ((0 != setParams(bitRate, dataBit, parity, stopBit)) &&
            (0 != setFlowCtrl(flowCtrl)))
        {
//            logErr( "tools::os::Serport - Configure device" );
            return;
        }

        // Configure timeouts
        if (!::SetCommTimeouts(_hComm, &_commTimeouts))
        {
//            logErr( "tools::os::Serport - SetCommTimeouts Error" );
            return;
        }

        ready = true;
    }

    // Events
    string stmp = "read.";
    stmp.append(_portName);
    _readEvent = ::CreateEvent(NULL, true, false, stmp.c_str());
    if (!_readEvent)
    {
//        logErr( "tools::os::Serport - create readEvent %08x", ::GetLastError() );
        return;
    }
    stmp = "write.";
    stmp.append(_portName);
    _writeEvent = ::CreateEvent(NULL, true, false, stmp.c_str());
    if (!_writeEvent)
    {
//        logErr( "tools::os::Serport - create writeEvent %08x", ::GetLastError() );
        return;
    }

    // Switch the port to ready if everything is OK
    changePortReady(ready);
}

/**
 * Destructor
 */
Serport::~Serport()
{
    // Completes reading and writing tasks
    cancel();

    closePort();

    if (_readEvent)
        ::CloseHandle(_readEvent);
    if (_writeEvent)
        ::CloseHandle(_writeEvent);
}

int Serport::setParams(ISerport::BitRate bRate, ISerport::DataBit nData,
                                ISerport::Parity parity, ISerport::StopBit nStop)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    switch (bRate)
    {
        case Serport::BitRate::BITRATE_2400:
            _dcb.BaudRate = CBR_2400;
            break;
        case Serport::BitRate::BITRATE_4800:
            _dcb.BaudRate = CBR_4800;
            break;
        case Serport::BitRate::BITRATE_9600:
            _dcb.BaudRate = CBR_9600;
            break;
        case Serport::BitRate::BITRATE_19200:
            _dcb.BaudRate = CBR_19200;
            break;
        case Serport::BitRate::BITRATE_38400:
            _dcb.BaudRate = CBR_38400;
            break;
        case Serport::BitRate::BITRATE_57600:
            _dcb.BaudRate = CBR_57600;
            break;
        case Serport::BitRate::BITRATE_115200:
            _dcb.BaudRate = CBR_115200;
            break;
        default:
            _dcb.BaudRate = CBR_19200;
            break;
    }

    switch (nData)
    {
        case Serport::DataBit::DATABIT_5:
            _dcb.ByteSize = 5;
            break;
        case Serport::DataBit::DATABIT_6:
            _dcb.ByteSize = 6;
            break;
        case Serport::DataBit::DATABIT_7:
            _dcb.ByteSize = 7;
            break;
        case Serport::DataBit::DATABIT_8:
            _dcb.ByteSize = 8;
            break;
        default:
            _dcb.ByteSize = 8;
            break;
    }

    switch (parity)
    {
        case Serport::Parity::PARITY_NONE:
            _dcb.fParity = FALSE;
            _dcb.Parity  = NOPARITY;
            break;
        case Serport::Parity::PARITY_EVEN:
            _dcb.fParity = TRUE;
            _dcb.Parity  = EVENPARITY;
            break;
        case Serport::Parity::PARITY_ODD:
            _dcb.fParity = TRUE;
            _dcb.Parity  = ODDPARITY;
            break;
        case Serport::Parity::PARITY_MARK:
            _dcb.fParity = TRUE;
            _dcb.Parity  = MARKPARITY;
            break;
        case Serport::Parity::PARITY_SPACE:
            _dcb.fParity = TRUE;
            _dcb.Parity  = SPACEPARITY;
            break;
        default:
            _dcb.fParity = FALSE;
            _dcb.Parity  = NOPARITY;
            break;
    }

    switch (nStop)
    {
        case StopBit::STOPBIT_1:
            _dcb.StopBits = ONESTOPBIT;
            break;
        case StopBit::STOPBIT_15:
            _dcb.StopBits = ONE5STOPBITS;
            break;
        case StopBit::STOPBIT_2:
            _dcb.StopBits = TWOSTOPBITS;
            break;
        default:
            _dcb.StopBits = ONESTOPBIT;
            break;
    }

    return configurePort();
}

/**
 * Set flow control parameters
 */
int Serport::setFlowCtrl(FlowCtrl flowCtrl)
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    switch (flowCtrl)
    {
        case Serport::FlowCtrl::FLOW_CTRL_NONE:
            _dcb.fOutX = false;
            _dcb.fInX  = false;
            break;
        case Serport::FlowCtrl::FLOW_CTRL_XON_XOFF:
            _dcb.fOutX = true;
            _dcb.fInX  = true;
            break;
        default:
            _dcb.fOutX = false;
            _dcb.fInX  = false;
            break;
    }

    return configurePort();
}

/**
 * Write data
 */
int Serport::write(const char* buffer, unsigned int size)
{
    const std::lock_guard<std::mutex> lock(_mutexWrite);

    // If the port is not ready, try to reconnect it
    {
        const std::lock_guard<std::recursive_mutex> rlock(_mutex);
        if (!_bPortReady && (0 != reset()))
        {
            return -1;
        }
    }

    // Writing with cancel management
    OVERLAPPED overlapped;
    overlapped.hEvent     = _writeEvent;
    overlapped.Offset     = 0;
    overlapped.OffsetHigh = 0;
    _canceledWrite       = false;
    int res               = ::WriteFile(_hComm, buffer, size, NULL, &overlapped);
    if ((0 == res) && (ERROR_IO_PENDING == (errno = ::GetLastError())))
    {
        WaitForSingleObject(_writeEvent, INFINITE);
        if (_canceledWrite)
        {
            ::CancelIo(_hComm);
//            logInfo( "Serport::write canceled" );
        }
        res = 1;
    }

    // Obtaining the result
    unsigned long nbBytes = 0;
    if (0 != res)
    {
        BOOL ovlres = ::GetOverlappedResult(_hComm, &overlapped, &nbBytes, TRUE);
        if (0 != ovlres)
        {
            if (0 != nbBytes)
            {
//                logInfo(TxPrefix, buffer, nbBytes ); // TODO
            }
        }
        else
        {
            errno = ::GetLastError();
        }
    }

    // Error reporting
    if (0 == res)
    {
        const std::lock_guard<std::recursive_mutex> rlock(_mutex);
        changePortReady(false);
//        logErr( "Serport::write ERROR %08x (size: %d)", errno, size );
    }

    return nbBytes;
}

/**
 * @brief Read data (blocking)
 *
 * @param buffer
 * @param size
 * @return
 */
int Serport::read(char* buffer, unsigned int size)
{
    return internalRead(buffer, size, 0);
}

/**
 * @brief Read data (with a time-out)
 *
 * @param buffer
 * @param size
 * @param delay
 * @return
 */
int Serport::read(char* buffer, unsigned int size, util::chrono::Delay delay)
{
    const auto delayMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(delay.toNanoseconds());
    return internalRead(buffer, size, static_cast<unsigned long>(delayMs.count()));
}

/**
 * @brief Reset the port
 *
 * @return
 */
int Serport::reset()
{
    const std::lock_guard<std::recursive_mutex> lock(_mutex);

    closePort();

    if (openPort("\\\\.\\" + _portName) == -1)
    {
//        logInfo( "reset - openPort" );
        return -1;
    }

    // Reconfigure
    if (configurePort() == -1)
    {
//        logInfo( "reset - configurePort" );
        return -1;
    }

    // Reconfiguring timeouts
    if (!::SetCommTimeouts(_hComm, &_commTimeouts))
    {
//        logInfo( "reset - SetCommTimeouts Error" );
        return -1;
    }

    // Serial port is ready
    changePortReady(true);
    return 0;
}

/**
 * @brief Obtain the number of chars in the read buffer. Not implemented
 *
 * @return
 */
int Serport::getNRead()
{
    return -1;
}

/**
 * @brief Obtain the number of chars in the write buffer. Not implemented
 *
 * @return
 */
int Serport::getNWrite()
{
    return -1;
}

/**
 * @brief Clear the read and write buffers
 *
 * @return
 */
int Serport::flush()
{
    BOOL res = ::PurgeComm(_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    if (0 == res)
    {
//        errno = ::GetLastError();
        return -1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Clear the write buffer
 *
 * @return
 */
int Serport::wflush()
{
    BOOL res = ::PurgeComm(_hComm, PURGE_TXCLEAR);
    if (0 == res)
    {
//        errno = ::GetLastError();
        return -1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Clear the read buffer
 *
 * @return
 */
int Serport::rflush()
{
    BOOL res = ::PurgeComm(_hComm, PURGE_RXCLEAR);
    if (0 == res)
    {
//        errno = ::GetLastError();
        return -1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Cancel a blocking operation (read/write)
 *
 * @return
 */
int Serport::cancel()
{
    _canceledRead  = true;
    _canceledWrite = true;
    ::SetEvent(_readEvent);
    ::SetEvent(_writeEvent);
    return 0;
}

// Internal methods

/**
 * @brief Open the COM port
 *
 * @param portname
 * @return
 */
int Serport::openPort(const std::string& portname)
{
    _hComm = CreateFile(portname.c_str(),
                         GENERIC_READ | GENERIC_WRITE,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,
                         NULL);
    if (_hComm == INVALID_HANDLE_VALUE)
    {
//        logInfo( "tools::os::openPort - open [%s] ERROR %08x ", portname, ::GetLastError() );
        return -1;
    }

    return 0;
}

/**
 * @brief Configure the COM port
 *
 * @return
 */
int Serport::configurePort()
{
    // Assigns the config to the port
    if (!::SetCommState(_hComm, &_dcb))
    {
//        logError( "tools::os::ConfigurePort - SetCommState ERROR %08x", ::GetLastError() );
        return -1;
    }

    return 0;
}

/**
 * @brief Close the COM port
 *
 */
void Serport::closePort()
{
    if (_hComm)
    {
        CloseHandle(_hComm);
    }
    _hComm = NULL;
}

/**
 * Read chars from the port
 *
 * @param[in] buffer where to store data
 * @param[size] how many chars to read
 * @param[in] inter-char timeout in milliseconds or zero for blocking read
 */
int Serport::internalRead(char* buffer, unsigned int size, unsigned long tout)
{
    const std::lock_guard<std::mutex> lock(_mutexRead);

    {
        const std::lock_guard<std::recursive_mutex> rlock(_mutex);

        // If the port is not ready, try to reconnect it
        if (!_bPortReady && (0 != reset()))
        {
            return -1;
        }

        // Reconfigure the read timeout if necessary
        if ((DWORD)tout != _commTimeouts.ReadTotalTimeoutConstant)
        {
            _commTimeouts.ReadIntervalTimeout        = MAXDWORD;
            _commTimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
            _commTimeouts.ReadTotalTimeoutConstant   = tout;
            if (!::SetCommTimeouts(_hComm, &_commTimeouts))
            {
                changePortReady(false);
                //                logError( "tools::os::internalRead - SetCommTimeouts ERROR %08x", ::GetLastError() );
                return -1;
            }
        }
    }

    // Reading with cancel management
    OVERLAPPED overlapped;
    overlapped.hEvent     = _readEvent;
    overlapped.Offset     = 0;
    overlapped.OffsetHigh = 0;
    _canceledRead        = false;
    int res               = ::ReadFile(_hComm, buffer, size, NULL, &overlapped);
    if ((0 == res) && (ERROR_IO_PENDING == (errno = ::GetLastError())))
    {
        WaitForSingleObject(_readEvent, INFINITE);
        if (_canceledRead)
        {
            ::CancelIo(_hComm);
//            logInfo( "Serport::internalRead canceled" );
        }
        res = 1;
    }

    // Getting the result
    unsigned long nbBytes = 0;
    if (0 != res)
    {
        BOOL ovlres = ::GetOverlappedResult(_hComm, &overlapped, &nbBytes, TRUE);
        if (0 != ovlres)
        {
            if (0 != nbBytes)
            {
//                logReadData( RxPrefix, buffer, nbBytes );
            }
        }
        else
        {
            errno = ::GetLastError();
        }
    }

    // Error reporting
    if (0 == res)
    {
//        logError( "Serport::internalRead ERROR %08x", errno );

        const std::lock_guard<std::recursive_mutex> rlock(_mutex);
        changePortReady(false);
    }

    return nbBytes;
}

/**
 * @brief Change the port ready state and notify listeners accordingly
 * The _mutex shall be locked when calling this method
 *
 * @param ready
 */
void Serport::changePortReady(bool ready)
{
    if (_bPortReady == ready)
        return;

    _bPortReady = ready;
    if (!_bPortReady)
    {
//        logError( "changePortReady: Port NOT Ready" );
    }
    else
    {
//        logInfo( "changePortReady: Port is ready" );
    }

    // TODO - Observer
    //    m_listenerManager.lock();
    //    for ( listenerIterator iter = beginListener();
    //          iter != endListener();
    //          ++iter )
    //    {
    //        if ( _bPortReady )
    //            ( *iter )->ready();
    //        else
    //            ( *iter )->notReady();
    //    }
    //    m_listenerManager.unlock();
}

Serport::Serport(ApplicationServices& app, Node node) :
    Serport(node.at("DeviceName").value<std::string>())
{
    (void)app;
}

FOUNDATION_FACTORY_REGISTER(tools::os::serport::Serport,
                            "tools::os::serport::Serport",
                            tools_os_serport_Serport)
