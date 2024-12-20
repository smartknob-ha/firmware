#include "../include/ux.hpp"

void UX::start() {
}

void UX::run() {

    for (;;) {
        switch (m_state) {
            case State::SETUP:
                setup();
                break;
            default:
                break;
        }
    }
}

void UX::setup() {
    // Turn ring lights on

    // Turn display on

    // Calibrate strain sensor

    // Calibrate magnetic encoder

    //
}