#pragma once
#include <Arduino.h>
#include <atomic>

struct Motor {
  const uint8_t p_dir;
  const uint8_t p_step;
  const uint8_t steps_per_rev;
  uint8_t dir;
  uint16_t step_offset;

  Motor(uint8_t p_dir, uint8_t p_step, uint8_t steps_per_rev, uint8_t dir, uint16_t step_offset) :
  p_dir(p_dir), p_step(p_step), steps_per_rev(steps_per_rev), dir(dir), step_offset(step_offset)
  {}
};

Motor* initMotor();
/* motor config */
void setStepOffset(Motor* motor, uint16_t offset);
void motorInvertdir(Motor* motor);
/* actually running the motor */
void stepMotor(Motor* motor);
void revolMotor(Motor* motor);
extern std::atomic<uint32_t> g_step;
