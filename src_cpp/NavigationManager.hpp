#ifndef NAVIGATION_MANAGER_HPP
#define NAVIGATION_MANAGER_HPP

enum class NavMode { MANUAL, AUTOMATIC };

class NavigationManager {
private:
    NavMode currentMode;
    double targetSpeed;

public:
    NavigationManager();
    
    void updateMode(bool cmdAuto, bool cmdMan);
    
    void processInputs(double joystickSpeed, bool btnStop, bool slowDown);

    NavMode getMode() const { return currentMode; }
    double getTargetSpeed() const { return targetSpeed; }
};

#endif