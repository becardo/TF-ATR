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

// Lógica de Movimentação com Redução de Velocidade para Inspeção
void NavigationManager::processInputs(double joystickSpeed, bool btnStop, bool slowDown) {
    // 1. Prioridade Máxima: Botão de Emergência / Parada 
    if (btnStop) {
        targetSpeed = 0.0;
        return;
    }

    if (currentMode == NavMode::MANUAL) {
        targetSpeed = joystickSpeed;
    } else {
        targetSpeed = 5.0; 
    }

    if (slowDown) {
        // Reduz para 20% da velocidade atual ou um valor fixo baixo (ex: 1.0)
        targetSpeed = 1.0; 
    }
}