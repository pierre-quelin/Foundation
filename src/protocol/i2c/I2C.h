/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file I2C.h
 * @brief Inter-IC or I2Cbus Interface.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory> // shared_ptr
#include <vector>

namespace protocol
{

namespace i2c
{

class I2CMaster
{
public:
    virtual ~I2CMaster()                   = default;
    I2CMaster()                            = default;
    I2CMaster(const I2CMaster&)            = delete;
    I2CMaster& operator=(const I2CMaster&) = delete;
    I2CMaster(I2CMaster&&)                 = delete;
    I2CMaster& operator=(I2CMaster&&)      = delete;

    /**
     * @brief Initializes the component. Necessary before its use or in case of error.
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int init() { return -1; }
    virtual int busCycleBegin() { return 0; }
    virtual int busCycleEnd() { return 0; }

    enum class Speed
    {
        _10kbs  = 10,   /**< Low-speed mode */
        _100kbs = 100,  /**< Std mode */
        _200kbs = 200,  /**< */
        _400kbs = 400,  /**< Fast mode */
        _1mbs   = 1000, /**< Fast mode+ */
        _17mbs  = 1700, /**< High-speed mode */
        _34mbs  = 3400, /**< High-speed mode */
    };

    /**
     * @brief Set the I2C/SMBus.
     *
     * @param speed I2C bus speed (min:_100kbs max:_400kbs).
     * @return 0 if successful, -1 otherwise.
     */
    virtual int setSpeed(Speed speed)
    {
        (void)speed;
        return -1;
    }

    // I2C - 7 bits slave address.
    /**
     * @brief Read I2C data from a slave.
     *
     * @param addr The 7 bits slave address.
     * @param buf Data bytes read from the slave.
     * @param len The number of bytes to read from the slave.
     * @return Number of read bytes or -1 on error.
     */
    virtual int /* ssize_t */ read(uint8_t addr, uint8_t* buf, size_t len)
    {
        (void)addr;
        (void)buf;
        (void)len;
        return -1;
    }

    /**
    * @brief Write I2C data to a slave.
    *
    * @param addr The 7 bit slave address.
    * @param buf Data bytes write to the slave.
    * @param len The number of bytes to write to the slave.
    * @return Number of write bytes or -1 on error.
    */
    virtual int /* ssize_t */ write(uint8_t addr, const uint8_t* buf, size_t len)
    {
        (void)addr;
        (void)buf;
        (void)len;
        return -1;
    }

    // TODO - Generalization
    //   class Msg
    //   {
    //       uint16_t addr;
    //       uint16_t flags;
    //   #define I2C_M_RD        0x0001  /* guaranteed to be 0x0001! */
    //   #define I2C_M_TEN       0x0010  /* use only if I2C_FUNC_10BIT_ADDR */
    //   #define I2C_M_DMA_SAFE      0x0200  /* use only in kernel space */
    //   #define I2C_M_RECV_LEN      0x0400  /* use only if I2C_FUNC_SMBUS_READ_BLOCK_DATA */
    //   #define I2C_M_NO_RD_ACK     0x0800  /* use only if I2C_FUNC_PROTOCOL_MANGLING */
    //   #define I2C_M_IGNORE_NAK    0x1000  /* use only if I2C_FUNC_PROTOCOL_MANGLING */
    //   #define I2C_M_REV_DIR_ADDR  0x2000  /* use only if I2C_FUNC_PROTOCOL_MANGLING */
    //   #define I2C_M_NOSTART       0x4000  /* use only if I2C_FUNC_NOSTART */
    //   #define I2C_M_STOP      0x8000  /* use only if I2C_FUNC_PROTOCOL_MANGLING */
    //       uint16_t len;
    //       uint8_t *buf;
    //   };
    //   virtual int transfer(uint8_t slaveAddr, std::vector<Msg>& msgs);

    // SMBus - 7 bits slave address.
    /**
    * @brief SMBus "read byte" protocol
    *
    * @param addr The 7 bit slave address.
    * @param cmd The device command
    * @param value The byte received from the device.
    * @return 0 if successful, -1 otherwise.
    */
    virtual int readByte(uint8_t addr, uint8_t cmd, uint8_t& value)
    {
        (void)addr;
        (void)cmd;
        (void)value;
        return -1;
    }

    /**
    * @brief SMBus "read word" protocol
    *
    * @param addr The 7 bit slave address.
    * @param cmd The device command
    * @param value The word received from the device.
    * @return 0 if successful, -1 otherwise.
    */
    virtual int readWord(uint8_t addr, uint8_t cmd, uint16_t& value)
    {
        (void)addr;
        (void)cmd;
        (void)value;
        return -1;
    }

    /**
    * TODO - To be defined
    *
    * @return 0 if successful, -1 otherwise.
    */
    virtual int readBlock(uint8_t addr, uint8_t cmd, std::vector<uint8_t>& values)
    {
        (void)addr;
        (void)cmd;
        (void)values;
        return -1;
    }

    /**
    * @brief SMBus "write byte" protocol
    *
    * @param addr The 7 bit slave address.
    * @param cmd The device command
    * @param value The byte to be written
    * @return 0 if successful, -1 otherwise.
    */
    virtual int writeByte(uint8_t addr, uint8_t cmd, uint8_t value)
    {
        (void)addr;
        (void)cmd;
        (void)value;
        return -1;
    }

    /**
    * @brief SMBus "write word" protocol
    *
    * @param addr The 7 bit slave address.
    * @param cmd The device command
    * @param value The word to be written
    * @return 0 if successful, -1 otherwise.
    */
    virtual int writeWord(uint8_t addr, uint8_t cmd, uint16_t value)
    {
        (void)addr;
        (void)cmd;
        (void)value;
        return -1;
    }

    /**
    * TODO - To be defined
    *
    * @return 0 if successful, -1 otherwise.
    */
    virtual int writeBlock(uint8_t addr, uint8_t cmd, const std::vector<uint8_t>& values)
    {
        (void)addr;
        (void)cmd;
        (void)values;
        return -1;
    }
};

class I2CSlave
{
public:
    virtual ~I2CSlave() = default;
    I2CSlave(const std::shared_ptr<I2CMaster>& master, uint8_t addr) :
        _addr(addr),
        _master(master) {}
    I2CSlave(const I2CSlave&)            = delete;
    I2CSlave& operator=(const I2CSlave&) = delete;
    I2CSlave(I2CSlave&&)                 = delete;
    I2CSlave& operator=(I2CSlave&&)      = delete;

    virtual int init() { return 0; }
    virtual int busCycleBegin() { return 0; }
    virtual int busCycleEnd() { return 0; }

    // I2C
    /**
     * @brief Read I2C data from a slave.
     *
     * @param buf Data bytes read from the slave.
     * @param len The number of bytes to read from the slave.
     * @return Number of read bytes or -1 on error.
     */
    virtual /* ssize_t */ int read(uint8_t* buf, size_t len) { return _master->read(_addr, buf, len); }

    /**
     * @brief Write I2C data to a slave.
     *
     * @param buf Data bytes write to the slave.
     * @param len The number of bytes to write to the slave.
     * @return Number of write bytes or -1 on error.
     */
    virtual /* ssize_t */ int write(const uint8_t* buf, size_t len) { return _master->write(_addr, buf, len); }

    // SMBus
    /**
     * @brief SMBus "read byte" protocol
     *
     * @param cmd The device command
     * @param value The byte received from the device.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int readByte(uint8_t cmd, uint8_t& value) { return _master->readByte(_addr, cmd, value); }

    /**
     * @brief SMBus "read word" protocol
     *
     * @param cmd The device command
     * @param value The word received from the device.
     * @return 0 if successful, -1 otherwise.
     */
    virtual int readWord(uint8_t cmd, uint16_t& value) { return _master->readWord(_addr, cmd, value); }
    /**
     * TODO - To be defined
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int readBlock(uint8_t cmd, std::vector<uint8_t>& values) { return _master->readBlock(_addr, cmd, values); }
    /**
     * @brief SMBus "write byte" protocol
     *
     * @param cmd The device command
     * @param value The byte to be written
     * @return 0 if successful, -1 otherwise.
     */
    virtual int writeByte(uint8_t cmd, uint8_t value) { return _master->writeByte(_addr, cmd, value); }
    /**
     * @brief SMBus "write word" protocol
     *
     * @param cmd The device command
     * @param value The word to be written
     * @return 0 if successful, -1 otherwise.
     */
    virtual int writeWord(uint8_t cmd, uint16_t value) { return _master->writeWord(_addr, cmd, value); }
    /**
     * TODO - To be defined
     *
     * @return 0 if successful, -1 otherwise.
     */
    virtual int writeBlock(uint8_t cmd, const std::vector<uint8_t>& values) { return _master->writeBlock(_addr, cmd, values); }

private:
    uint8_t _addr;
    std::shared_ptr<I2CMaster> _master;
};

} // namespace i2c

} // namespace protocol