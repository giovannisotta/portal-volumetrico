#include "motor.h"

Motor* initMotor() {
  Motor* motor = new Motor(DIR, STEP, STEPS_PER_REV, 0, 5000); 

  pinMode(motor->p_dir, OUTPUT);
  pinMode(motor->p_step, OUTPUT);

  digitalWrite(motor->p_dir, motor->dir);

  return motor;
}

/* motor config */
void setStepOffset(Motor* motor, uint16_t offset) {
  motor->step_offset = offset;
}

void motorInvertdir(Motor* motor) {
  motor->dir = !(motor->dir);
  digitalWrite(motor->p_dir, motor->dir);
}

/* actually running the motor */
void stepMotor(Motor* motor) {
  digitalWrite(motor->p_step, HIGH);
  delayMicroseconds(motor->step_offset);

  digitalWrite(motor->p_step, LOW);
  delayMicroseconds(motor->step_offset);
}

void revolMotor(Motor* motor) {
  int i;

  for (i = 0; i < motor->steps_per_rev; i++) {
    stepMotor(motor);
  }
}
