#include "PIDController.hpp"
#include <algorithm> 

PIDController::PIDController(double kp, double ki, double kd, double sample_time)
    : Kp(kp), Ki(ki), Kd(kd), dt(sample_time), integral(0.0), last_error(0.0), output_limit(100.0) {}

void PIDController::setGains(double kp, double ki, double kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
}

void PIDController::reset() {
    integral = 0.0;
    last_error = 0.0;
}

double PIDController::compute(double setpoint, double current_value) {
    double error = setpoint - current_value;


    double P = Kp * error;

    integral += error * dt;
    double I = Ki * integral;

    double derivative = (error - last_error) / dt;
    double D = Kd * derivative;

    last_error = error;

    double output = P + I + D;

    if (output > output_limit) {
        output = output_limit;
        integral -= error * dt; 
    } else if (output < -output_limit) {
        output = -output_limit;
        integral -= error * dt;
    }

    return output;
}