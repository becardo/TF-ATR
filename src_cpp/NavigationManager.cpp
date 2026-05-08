#include "NavigationManager.hpp"

// Bloco: Comando de Navegação

// ----- Construtor da Classe NavigationManager -----
// currentMode(NavMode::MANUAL): liga o robô já no modo manual.
// targetSpeed(0.0): setpoint em velocidade = 0.0.
NavigationManager::NavigationManager() : currentMode(NavMode::MANUAL), targetSpeed(0.0) {}

// Seleção: Manual / Automático
void NavigationManager::updateMode(bool cmdAuto, bool cmdMan) {
    if (cmdAuto) {
        currentMode = NavMode::AUTOMATIC;
    } else if (cmdMan) {
        currentMode = NavMode::MANUAL;
    }
}

// Acelerador e Freio
void NavigationManager::processInputs(double joystickSpeed, bool btnStop) {
    // Botão de Emergencia: robô para
    if (btnStop) {
        targetSpeed = 0.0;
        return;
    }

    if (currentMode == NavMode::MANUAL) {
        // Velocidade vem do controle joystickSeed
        targetSpeed = joystickSpeed;
    } else {
        // Velocidade automática: 5km/h
        targetSpeed = 5.0; 
    }
}