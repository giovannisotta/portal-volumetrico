#pragma once
#include "calc.h"
#include "motor.h"

bool measureBaseline(Calib &c);
BoxResult acquireBox(Motor* motor, const Calib &calib);
