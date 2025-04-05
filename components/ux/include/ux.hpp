#ifndef UX_HPP
#define UX_HPP

#include <Component.hpp>

#include "LightSensor.hpp"
#include "MotorDriver.hpp"
#include "RingLights.hpp"
#include "StrainSensor.hpp"
#include "DisplayDriver.hpp"

class UX final : public sdk::Component {
public:
    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          stop() override;
    Status          run() override;

private:
    static constexpr char TAG[]    = "UX";
    Status                m_status = Status::UNINITIALIZED;

    ringLights::RingLights& ringLights();
    LightSensor&            lightSensor();
    MotorDriver&            motorDriver();
    StrainSensor&           strainSensor();
    DisplayDriver&          displayDriver();

};


#endif //UX_HPP
