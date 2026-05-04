#include "NavigationManager.hpp"

NavigationManager::NavigationManager() : currentMode(NavMode::MANUAL), targetSpeed(0.0) {}

void NavigationManager::updateMode(bool cmdAuto, bool cmdMan) {
    if (cmdAuto) {
        currentMode = NavMode::AUTOMATIC;
    } else if (cmdMan) {
        currentMode = NavMode::MANUAL;
    }
}

void NavigationManager::processInputs(double joystickSpeed, bool btnStop) {
    if (btnStop) {
        targetSpeed = 0.0;
        return;
    }

    if (currentMode == NavMode::MANUAL) {
        targetSpeed = joystickSpeed;
    } else {
        targetSpeed = 5.0; 
    }
}