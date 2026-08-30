#ifndef __PID_H
#define __PID_H

typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float setpoint;       // Ä¿±êÖµ
    float integral;
    float prev_error;
    float integral_limit;
    float output_limit;
} PID_HandleTypeDef;

void PID_Init(PID_HandleTypeDef *pid, float kp, float ki, float kd);
float PID_Calculate(PID_HandleTypeDef *pid, float input);
void PID_Reset(PID_HandleTypeDef *pid);

#endif
