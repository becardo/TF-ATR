#include "NavigationManager.hpp"

// Bloco: Comando de Navegação

// ----- Construtor da Classe NavigationManager -----
NavigationManager::NavigationManager() : currentMode(NavMode::MANUAL), targetSpeed(0.0) {}

// Seleção: Manual / Automático
void NavigationManager::updateMode(bool cmdAuto, bool cmdMan) {
    if (cmdAuto) {
        currentMode = NavMode::AUTOMATIC;
    } else if (cmdMan) {
        currentMode = NavMode::MANUAL;
    }
}

// Acelerador e Freio com lógica de redução de velocidade (slowDown)
void NavigationManager::processInputs(double joystickSpeed, bool btnStop, bool slowDown) {
    // 1. Botão de Emergência: Prioridade máxima, o robô para.
    if (btnStop) {
        targetSpeed = 0.0;
        return;
    }

    // Lógica de Inspeção (slowDown): Se a câmera estiver processando,
    // a velocidade cai para um valor baixo de 0.2m/s para garantir a qualidade
    if (slowDown) {
        targetSpeed = 0.2; 
    } 
    else {
        // Operação Normal
        if (currentMode == NavMode::MANUAL) {
            // Velocidade vem do controle joystick
            targetSpeed = joystickSpeed; 
        } else {
            // Velocidade automática nominal: 1.0m/s
            targetSpeed = 1.0; 
        }
    }
}

NavMode NavigationManager::getMode() const {
    return currentMode;
}

double NavigationManager::getTargetSpeed() const {
    return targetSpeed;
}