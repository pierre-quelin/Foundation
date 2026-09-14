/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file PCA9635.h
 * @brief The PCA9635 is an I2C-bus controlled 16-bit LED driver optimized for
 *
 * Red/Green/Blue/Amber (RGBA) color mixing applications.
 *
 * See https://www.nxp.com/docs/en/data-sheet/PCA9635.pdf
 */
#pragma once

#include "protocol/i2c/I2C.h"
#include "tools/design/factory/IObject.hpp"
#include "util/logger/Logger.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

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

class PCA9635 final : public tools::design::factory::IObject, public protocol::i2c::I2CSlave
{
public:
    ~PCA9635() override = default;
    PCA9635(tools::design::ApplicationServices& app,
            tools::design::config::Node node);
    PCA9635(const PCA9635&)            = delete;
    PCA9635& operator=(const PCA9635&) = delete;
    PCA9635(PCA9635&&)                 = delete;
    PCA9635& operator=(PCA9635&&)      = delete;

    // Power_state
    /**
     * @brief Set power state to low power mode Off.
     *
     * @return 0 if successful, -1 otherwise.
     */
    int wakeUp();
    /**
     * @brief Set power state to low power mode On.
     *
     * @return 0 if successful, -1 otherwise.
     */
    int sleep();

    // Mode configuration
    enum class GrpCtrl
    {
        DIMMING  = 0,
        BLINKING = 1
    };
    /**
     * @brief Set the mode register 2 group control state
     *
     * @param grpCtrl The group control state
     * @return 0 if successful, -1 otherwise.
     */
    int setModeGrpCtrl(GrpCtrl grpCtrl = GrpCtrl::DIMMING);

    enum class LogicState
    {
        NOT_INVERTED = 0,
        INVERTED     = 1
    };
    /**
     * @brief Set the mode register 2 output logic state.
     *
     * @param state The output logical state.
     *      INVERTED : Value to use when external driver used.
     *      NOT_INVERTED : Value to use when no external driver used.
     * @return 0 if successful, -1 otherwise.
     */
    int setModeOutLogicState(LogicState state = LogicState::NOT_INVERTED);

    enum class Driver
    {
        OPEN_DRAIN = 0,
        TOTEM_POLE = 1
    };
    /**
     * @brief Set the mode register 2 output driver
     *
     * @param driver The output driver state
     *      TOTEM_POLE : The 16 LED outputs are configured with a totem pole structure.
     *      OPEN_DRAIN : The 16 LED outputs are configured with an open-drain structure.
     * @return 0 if successful, -1 otherwise.
     */
    int setModeOutDrv(Driver driver = Driver::TOTEM_POLE);

    enum class OELogicState
    {
        LOW    = 0,
        OUTDRV = 1,
        HIGH   = 2
    };
    /**
     * @brief Set the logical state of the 16 LEDn outputs when input OE=1.
     *
     * @param state The LEDn logical state.
     *      LOW : LEDn = 0
     *      OUTDRV : LEDn = 1 when setModeOutDrv(TOTEM_POLE), LEDn = high-impedance when setModeOutDrv(OPEN_DRAIN).
     *      HIGH : LEDn = high-impedance
     * @return 0 if successful, -1 otherwise.
     */
    int setModeOutNE(OELogicState state = OELogicState::OUTDRV);

    /**
     * @brief Set the group duty cycle control.
     * When dimming is programmed in mode register 2 group control (setModeGrpCtrl(DIMMING)), a 190 Hz fixed frequency signal is superimposed with the 97 kHz individual brightness control signal.
     * pwm is then used as a global brightness control allowing the LED outputs to be dimmed with the same value.
     *
     * @param pwm global brightness control.
     * @return 0 if successful, -1 otherwise.
     */
    int setDimmingGrpCtrl(uint8_t pwm = 0xFF);

    /**
     * @brief Set the group duty cycle control.
     * When blinking bit is programmed in mode register 2 group control (setModeGrpCtrl(BLINKING)), pwm and freq define a global blinking pattern.
     * freq contains the blinking period (from 24 Hz to 10.73 s) and pwm the duty cycle (ON/OFF ratio in %).
     *
     * @param pwm blinking duty cycle.
     * @param freq blinking period.
     * @return 0 if successful, -1 otherwise.
     */
    int setBlinkingGrpCtrl(uint8_t pwm = 0xFF, uint8_t freq = 0);

    enum class Led
    {
        LED0  = 0x00,
        LED1  = 0x01,
        LED2  = 0x02,
        LED3  = 0x03,
        LED4  = 0x04,
        LED5  = 0x05,
        LED6  = 0x06,
        LED7  = 0x07,
        LED8  = 0x08,
        LED9  = 0x09,
        LED10 = 0x0A,
        LED11 = 0x0B,
        LED12 = 0x0C,
        LED13 = 0x0D,
        LED14 = 0x0E,
        LED15 = 0x0F
    };
    enum class LedDriver
    {
        LDRx_OFF     = 0x00, // LED driver x is off (default power-up state).
        LDRx_ON      = 0x01, // LED driver x is fully on (individual brightness and group dimming/blinking not controlled).
        LDRx_BRI     = 0x02, // LED driver x individual brightness can be controlled through its PWMx register.
        LDRx_BRI_BLK = 0x03  // LED driver x individual brightness and group dimming/blinking can be
                             // controlled through its PWMx register and the GRPPWM registers.
    };
    /**
     * @brief Set the individual driver output value
     *
     * @param led The led
     * @param driver The driver output value
     * @return 0 if successful, -1 otherwise.
     */
    int setLedDrvOut(Led led, LedDriver driver);
    /**
     * @brief Set all drivers output at the same value
     *
     * @param driver The driver value
     * @return 0 if successful, -1 otherwise.
     */
    int setLedDrvOut(LedDriver driver);
    /**
     * @brief Sets the led output drivers
     *
     * @param drivers The desired led drivers
     * @return 0 if successful, -1 otherwise.
     */
    int setLedDrvOut(const std::vector<std::pair<Led, LedDriver>>& drivers);

    /**
     * @brief Set individual led brightness
     *
     * @param led The led
     * @param pwm The brightness. 00h (0 % duty cycle = LED output off) to FFh (99.6 % duty cycle = LED output at maximum brightness).
     * @return 0 if successful, -1 otherwise.
     */
    int setBrightness(Led led, uint8_t pwm = 0);
    /**
     * @brief Set all leds brightness
     *
     * @param pwms the brightness for LED0 to LED15.
     * @return 0 if successful, -1 otherwise.
     */
    int setBrightness(const std::array<uint8_t, 16>& pwms);

private:
    PCA9635(const std::shared_ptr<protocol::i2c::I2CMaster>& master,
            const std::shared_ptr<util::logger::Logger>& logger,
            uint8_t addr);

    [[nodiscard]] util::logger::Logger& logger() const { return *_logger; }

    static uint8_t ledoutRegister(Led led);
    static int ledoutShift(Led led);
    static uint8_t packLedDriver(LedDriver driver);

    std::shared_ptr<util::logger::Logger> _logger;

    enum PCA9635_INC_OPTION
    {
        PCA9635_INC_NONE     = 0x00,    // no Auto-Increment
        PCA9635_INC_AUTO_ALL = 0x80,    // Auto-Increment for all registers. D4..D0
                                        // roll over to ‘00000’ after the last register (1 1011) is accessed.
        PCA9635_INC_AUTO_PWM = 0xA0,    // Auto-Increment for individual brightness registers only. D4..D0
                                        // roll over to ‘00010’ after the last register (1 0001) is accessed.
        PCA9635_INC_AUTO_GRP = 0xC0,    // Auto-Increment for global control registers only. D4..D0 roll over
                                        // to ‘10010’ after the last register (1 0011) is accessed.
        PCA9635_INC_AUTO_PWM_GRP = 0xE0 // Auto-Increment for individual and global control registers only. D4..D0
                                        // roll over to ‘00010’ after the last register (1 0011) is accessed.
    };

    enum class Register
    {
        MODE1      = 0x00, // read/write Mode register 1
        MODE2      = 0x01, // read/write Mode register 2
        PWM0       = 0x02, // read/write brightness control LED0
        PWM1       = 0x03, // read/write brightness control LED1
        PWM2       = 0x04, // read/write brightness control LED2
        PWM3       = 0x05, // read/write brightness control LED3
        PWM4       = 0x06, // read/write brightness control LED4
        PWM5       = 0x07, // read/write brightness control LED5
        PWM6       = 0x08, // read/write brightness control LED6
        PWM7       = 0x09, // read/write brightness control LED7
        PWM8       = 0x0A, // read/write brightness control LED8
        PWM9       = 0x0B, // read/write brightness control LED9
        PWM10      = 0x0C, // read/write brightness control LED10
        PWM11      = 0x0D, // read/write brightness control LED11
        PWM12      = 0x0E, // read/write brightness control LED12
        PWM13      = 0x0F, // read/write brightness control LED13
        PWM14      = 0x10, // read/write brightness control LED14
        PWM15      = 0x11, // read/write brightness control LED15
        GRPPWM     = 0x12, // read/write group duty cycle control
        GRPFREQ    = 0x13, // read/write group frequency
        LEDOUT0    = 0x14, // read/write LED output state 0 (LED0..LED3)
        LEDOUT1    = 0x15, // read/write LED output state 1 (LED4..LED7)
        LEDOUT2    = 0x16, // read/write LED output state 2 (LED8..LED11)
        LEDOUT3    = 0x17, // read/write LED output state 3 (LED12..LED15)
        SUBADR1    = 0x18, // read/write I2C-bus subaddress 1
        SUBADR2    = 0x19, // read/write I2C-bus subaddress 2
        SUBADR3    = 0x1A, // read/write I2C-bus subaddress 3
        ALLCALLADR = 0x1B  // read/write LED All Call I2C-bus address
    };

    enum PCA9635_MODE1_REGISTER
    {
        PCA9635_MODE1_AI2 = 0x80,    // R   0 Register Auto-Increment disabled
                                     //     1* Register Auto-Increment enabled
        PCA9635_MODE1_AI1 = 0x40,    // R   0* Auto-Increment bit 1 = 0
                                     //     1 Auto-Increment bit 1 = 1
        PCA9635_MODE1_AI0 = 0x20,    // R   0* Auto-Increment bit 0 = 0
                                     //     1 Auto-Increment bit 0 = 1
        PCA9635_MODE1_SLEEP = 0x10,  // R/W 0 Normal mode.
                                     //     1* Low power mode. Oscillator off.
        PCA9635_MODE1_SUB1 = 0x08,   // R/W 0* PCA9635 does not respond to I2C-bus subaddress 1.
                                     //     1 PCA9635 responds to I2C-bus subaddress 1.
        PCA9635_MODE1_SUB2 = 0x04,   // R/W 0* PCA9635 does not respond to I2C-bus subaddress 2.
                                     //     1 PCA9635 responds to I2C-bus subaddress 2.
        PCA9635_MODE1_SUB3 = 0x02,   // R/W 0* PCA9635 does not respond to I2C-bus subaddress 3.
                                     //     1 PCA9635 responds to I2C-bus subaddress 3.
        PCA9635_MODE1_ALLCALL = 0x01 // R/W 0 PCA9635 does not respond to LED All Call I2C-bus address.
                                     //     1* PCA9635 responds to LED All Call I2C-bus address.
    };

    enum PCA9635_MODE2_REGISTER
    {
        PCA9635_MODE2_RESERVED_1 = 0x80, // R   0* Reserved
        PCA9635_MODE2_RESERVED_2 = 0x40, // R   0* Reserved
        PCA9635_MODE2_DMBLNK     = 0x20, // R/W 0* Group control = dimming. GRPPWM is then used as a global brightness
                                         //     1  Group control = blinking. GRPPWM and GRPFREQ registers define a global blinking pattern
        PCA9635_MODE2_INVRT = 0x10,      // R/W 0* Output logic state not inverted. Value to use when no external driver used.
                                         //        Applicable when OE = 0.
                                         //     1  Output logic state inverted. Value to use when external driver used.
                                         //        Applicable when OE = 0.
        PCA9635_MODE2_OCH = 0x08,        // R/W 0* Outputs change on STOP command.
                                         //     1  Outputs change on ACK.
        PCA9635_MODE2_OUTDRV = 0x04,     // R/W 0  The 16 LED outputs are configured with an open-drain structure.
                                         //     1* The 16 LED outputs are configured with a totem pole structure.
        PCA9635_MODE2_OUTNE = 0x03,      // R/W 00  When OE = 1 (output drivers not enabled), LEDn = 0.
                                         //     01* When OE = 1 (output drivers not enabled):
                                         //         LEDn = 1 when OUTDRV = 1
                                         //         LEDn = high-impedance when OUTDRV = 0 (same as OUTNE[1:0] = 10)
                                         //     10  When OE = 1 (output drivers not enabled), LEDn = high-impedance.
                                         //     11  reserved
    };

    mutable std::mutex _mutex;
};

} // namespace driver::chip
