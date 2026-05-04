#ifndef PID_CONTROLLER_HPP
#define PID_CONTROLLER_HPP

class PIDController {
private:
    double Kp, Ki, Kd;
    double integral;
    double last_error;
    double dt;
    double output_limit;
    public:
    PIDController(double kp, double ki, double kd, double sample_time);
    double compute(double setpoint, double current_value);
    void reset();
    void setGains(double kp, double ki, double kd);
};

#endif