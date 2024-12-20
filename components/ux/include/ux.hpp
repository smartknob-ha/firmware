#ifndef FIRMWARE_COMPONENTS_UX_INCLUDE_UX_HPP
#define FIRMWARE_COMPONENTS_UX_INCLUDE_UX_HPP

#include "DisplayDriver.hpp"
#include "LightSensor.hpp"
#include "MagneticEncoder.hpp"
#include "MotorDriver.hpp"
#include "RightLights.hpp"
//#include "StrainSensor.hpp"

class UX {
public:
    UX(ringLights::RingLights& ring_lights,
       LightSensor&            light_sensor,
//       StrainSensor&           strain_sensor,
       MagneticEncoder&        magnetic_encoder,
       MotorDriver&            motor_driver,
       DisplayDriver&          display_driver)
        : m_ringLights(ring_lights),
          m_lightSensor(light_sensor),
//          m_strainSensor(strain_sensor),
          m_magneticEncoder(magnetic_encoder),
          m_motorDriver(motor_driver),
          m_displayDriver(display_driver){};

    void start();

    enum class State {
        SETUP,

    };


private:
    ringLights::RingLights& m_ringLights;
    LightSensor&            m_lightSensor;
//    StrainSensor&           m_strainSensor;
    MagneticEncoder&        m_magneticEncoder;
    MotorDriver&            m_motorDriver;
    DisplayDriver&          m_displayDriver;

    State m_state;

    [[noreturn]] void run();

    void setup();
};


#endif // FIRMWARE_COMPONENTS_UX_INCLUDE_UX_HPP
