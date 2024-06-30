#include "../include/Effects.hpp"

#include <math.h>

#include <cmath>

#include "esp_log.h"

namespace ringLights {

#define M_TAU (2 * M_PI)

#define DEGREE_PER_LED (360.0 / NUM_LEDS)
#define RAD_PER_LED (M_TAU / NUM_LEDS)

// Reset an angle to between 0 and +inf
#define WRAP_NEGATIVE_DEGREE(a) (a < 0.0f ? 360.0f + a : a)

#define GET_SMALLEST_DEGREE_DIFFERENCE(a, b) (180.0f - fabs(abs(a - b) - 180.0f))

#define DEGREE_TO_RADIAN(a) (a * M_PI / 180)

    // Just make sure to enter your angles in clockwise order
    int_fast16_t GET_CLOCKWISE_DIFF_DEGREES(int_fast16_t a, int_fast16_t b) {
        int_fast16_t diff = b - a;
        if (diff < 0) {
            return diff + 360;
        }
        return diff;
    }

    /* Calculates whether angle c is between angle a and b
     *		in a clockwise direction
     *  @returns how far the current angle is between a and b, clockwise.
     */
    int_fast16_t IS_BETWEEN_A_B_CLOCKWISE_DEGREES(int_fast16_t a, int_fast16_t b, int_fast16_t c) {
        int_fast16_t relativeTotalWidth = a + GET_CLOCKWISE_DIFF_DEGREES(a, b);
        int_fast16_t relativeAngle      = a + GET_CLOCKWISE_DIFF_DEGREES(a, c);

        return relativeAngle - relativeTotalWidth;
    }

    template<typename T>
    double GET_LED_ANGLE_DEGREES(T led) {
        static_assert(std::is_arithmetic<T>::value);
        double angle = static_cast<double>(led) * static_cast<double>(DEGREE_PER_LED);
        if (angle < 0.0) {
            angle = 360.0 + angle;
        }
        return angle;
    }

    template<typename T>
    double GET_LED_ANGLE_RAD(T led) {
        static_assert(std::is_arithmetic<T>::value);
        // (M_PI / 2) because radians start on the right (90 degrees)
        double angle = (led * RAD_PER_LED) + (M_PI / 2);
        if (angle < 0) {
            angle = M_TAU + angle;
        }
        if (angle > M_PI) {
            angle = angle - M_TAU;
        }
        return angle;
    }

    bool HSV_IS_EQUAL(hsv_t a, hsv_t b) {
        return a.h == b.h && a.s == b.s && a.v == b.v;
    }

    void effects::pointer(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        auto angleDegrees           = WRAP_NEGATIVE_DEGREE(std::fmod(msg.paramA, 360.0));
        auto widthDegree            = static_cast<float>(msg.paramB);
        auto widthHalfPointerDegree = static_cast<float>(widthDegree) / 2.0f;

        hsv_t color = msg.primaryColor;

        for (int_fast8_t i = 0; i < NUM_LEDS; i++) {
            float currentDegree   = static_cast<float>(i) * static_cast<float>(DEGREE_PER_LED);
            float degreesToCenter = GET_SMALLEST_DEGREE_DIFFERENCE(angleDegrees, currentDegree);
            float progress        = 0.0f;

            if (degreesToCenter <= widthHalfPointerDegree) {
                progress = (widthHalfPointerDegree - degreesToCenter) / widthHalfPointerDegree;
            }

            if (0.1f <= progress && progress <= 1.0f) {
                color.value = static_cast<uint8_t>(static_cast<double>(msg.primaryColor.value) * abs(progress));
            } else {
                color.value = 0;
            }

            buffer[i % NUM_LEDS] = hsv2rgb_rainbow(color);
            color.value          = 0;
        }
    }

    void effects::percent(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        int_fast16_t start = static_cast<int_fast16_t >(msg.paramA);
        int_fast16_t end   = msg.paramB;

        // Scale `end` to percentage
        double       percent    = static_cast<double>(msg.paramC) / 100;
        int_fast16_t totalWidth = GET_CLOCKWISE_DIFF_DEGREES(start, end);
        int_fast16_t correctEnd = start + (totalWidth * (percent));
        correctEnd %= 360;

        double degreePerPercent = (totalWidth / 100.0);

        // The vast majority of the LED's will be set using this colour, so avoid recalculating it for every LED
        rgb_t activeColor = hsv2rgb_rainbow(msg.primaryColor);

        for (int_fast8_t i = 0; i < NUM_LEDS; i++) {
            auto currentDegree = static_cast<int_fast16_t>(std::round(GET_LED_ANGLE_DEGREES(i)));
            auto remainder     = IS_BETWEEN_A_B_CLOCKWISE_DEGREES(start, correctEnd, currentDegree);
            if (remainder < 0 - degreePerPercent) {
                buffer[i] = activeColor;
            } else if (remainder <= 0) {
                auto value = static_cast<uint8_t>(std::round(static_cast<double>(msg.primaryColor.value) * (abs(remainder) / degreePerPercent)));
                buffer[i]  = hsv2rgb_rainbow({.h = msg.primaryColor.hue,
                                              .s = msg.primaryColor.sat,
                                              .v = value});
            } else {
                buffer[i] = {{0}, {0}, {0}};
            }
        }
    }

    void effects::fill(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        for (auto& led: buffer) {
            led = hsv2rgb_rainbow(msg.primaryColor);
        }
    }

    void effects::gradient(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        // Since every opposite LED gets the same color,
        // the gradient buffer only needs to be half the size
        // of the LED buffer.
        const static uint_fast8_t gradientResolution = NUM_LEDS * 2;

        static hsv_t pPrimaryColor, pSecondaryColor;
        static hsv_t gradientBuffer[gradientResolution];

        // Only recalculate the gradient if the colors changed
        if (!HSV_IS_EQUAL(msg.primaryColor, pPrimaryColor) || !HSV_IS_EQUAL(msg.secondaryColor, pSecondaryColor)) {
            hsv_fill_gradient2_hsv(gradientBuffer, gradientResolution, msg.secondaryColor, msg.primaryColor,
                                   COLOR_SHORTEST_HUES);
            pPrimaryColor   = msg.primaryColor;
            pSecondaryColor = msg.secondaryColor;
        }

        double gradientAngle = std::fmod(static_cast<double>(msg.paramA), 360.0);

        // Divide by 50 so 100 percent covers the whole unit circle height
        double gradientWidth = static_cast<double>(msg.paramB) / 50.0;
        // Subtract 1 so 50 percent will be 0, which is the center y of the unit circle
        double gradientCenter = (static_cast<double>(msg.paramC) / 50.0) - 1;

        // Center is along the middle line, so upper and lower describe the start and
        // ending for both the left and the right side of the gradient on the circle
        double upper = gradientCenter + (gradientWidth / 2);
        double lower = gradientCenter - (gradientWidth / 2);

        for (int_fast16_t i = 0; i < NUM_LEDS; i++) {
            double currentDegree = GET_LED_ANGLE_RAD(i);
            currentDegree -= DEGREE_TO_RADIAN(gradientAngle);
            double yPos = sin(currentDegree);

            if (yPos <= lower) {
                buffer[i] = hsv2rgb_rainbow(msg.secondaryColor);
            } else if (yPos >= upper) {
                buffer[i] = hsv2rgb_rainbow(msg.primaryColor);
            } else {
                double progress = (yPos - lower) / gradientWidth;
                buffer[i]       = hsv2rgb_rainbow(gradientBuffer[static_cast<int>(std::round(progress * (gradientResolution - 1)))]);
            }
        }
    }

    void effects::skip(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        for (int_fast16_t i = 0; i < NUM_LEDS; i++) {
            if (i % 2) {
                buffer[i] = hsv2rgb_rainbow(msg.primaryColor);
            } else {
                buffer[i] = hsv2rgb_rainbow(msg.secondaryColor);
            }
        }
    }

    void effects::rainbowUniform(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        static uint8_t hsv_step = 0;
        hsv_step++;
        hsv_t color{
                .hue        = hsv_step,
                .saturation = msg.primaryColor.saturation,
                .value      = msg.primaryColor.value};

        rgb_t rgb = hsv2rgb_rainbow(color);

        // Give all LED's the same colour
        for (auto& led: buffer) {
            led = rgb;
        }
    }

    void effects::rainbowRadial(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg) {
        static rgb_t rainbow_buffer[NUM_LEDS];
        static bool  ran = false;
        if (!ran) {
            float scalar = static_cast<float>(UINT8_MAX) / (static_cast<float>(NUM_LEDS) - 1);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                float hue         = std::fmin(static_cast<float>(i) * scalar, 255.0f);
                rainbow_buffer[i] = hsv2rgb_rainbow({.h = static_cast<uint8_t>(roundf(hue)), .s = 255, .v = 255});
                ran               = true;
            }
        }

        static int_fast16_t p_angle = 0;
        p_angle                     = (p_angle + static_cast<int_fast16_t >(round(msg.paramA))) % 360;
        p_angle                     = p_angle <= 0 ? p_angle + 360 : p_angle;

        auto led_offset = static_cast<int_fast16_t>(p_angle / DEGREE_PER_LED) % NUM_LEDS;

        for (int_fast16_t i = 0; i < NUM_LEDS; i++) {
            buffer[i] = rainbow_buffer[(i + led_offset) % NUM_LEDS];
        }
    }

} // namespace ringLights