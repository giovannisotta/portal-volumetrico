#pragma once
#include "secrets.h"

//motor
#define DIR             12
#define STEP            27
#define STEPS_PER_REV   200

//ToF sensors
#define I2C_SDA         21
#define I2C_SCL         22

#define XSHUT_LEFT      26
#define XSHUT_TOP       25
#define XSHUT_RIGHT     32

#define TOF_LEFT_ADDR   0x30
#define TOF_TOP_ADDR    0x31
#define TOF_RIGHT_ADDR  0x32

#define MARGEM_MM       20

//Samples
#define K_STEPS         20
#define MM_PER_STEP     0.455f
#define COMPR_MAX_MM    400
#define MAX_SAMPLES     200
#define N_FIM           3
#define MAX_FALHAS      25

//Calib
#define D_MM            270.0f
#define DS_MIN_MM       20.0f
#define FILTRO_MEDIANA  0
#define K_MENORES_MEDIANA 3
#define EXTRA_L0        57.0f
#define EXTRE_LK        0.12f

//Servidor
#define SERVER_URL      "http://10.177.227.149:5000/caixas"
#define MODO_SIMULACAO  1
#define SIM_INTERVALO_MS 3000
