#include "PIDController.hpp"
#include <algorithm> 

// Bloco: Controle de Navegação

// ----- Construtor da Classe PIDController -----
// integral(0.0): acumula o erro ao longo do tempo. Parcela I
// last_error(0.0): Parcela D.
// dt(sample_time): tempo de amostragem (Delta T).
// output_limit(100.0): PWM de 100%
PIDController::PIDController(double kp, double ki, double kd, double sample_time)
    : Kp(kp), Ki(ki), Kd(kd), integral(0.0), last_error(0.0), dt(sample_time), output_limit(100.0) {}

// Permite troca de parâmetros P, I e D com o robô em funcionamento.
void PIDController::setGains(double kp, double ki, double kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
}

// Troca de modo de Manual para Automático: zera tudo.
void PIDController::reset() {
    integral = 0.0;
    last_error = 0.0;
}

// Motor do PID
double PIDController::compute(double setpoint, double current_value) {
    // Onde eu quero estar (setpoint) - onde eu estou agora.
    double error = setpoint - current_value;

    // Proporcional 
    double P = Kp * error;

    // Soma do erro ao longo do tempo: Integral
    integral += error * dt;
    double I = Ki * integral;

    // Inclinação da reta do erro: Derivativo
    double derivative = (error - last_error) / dt;
    double D = Kd * derivative;

    // Atualiza a memória para o próximo ciclo
    last_error = error;

    double output = P + I + D;

    // Robo só aceita força de -100 a 100
    if (output > output_limit) {
        output = output_limit;
        integral -= error * dt; 
        
    } else if (output < -output_limit) {
        output = -output_limit;
        integral -= error * dt;
    }

    return output;
}