#include "pid.h"

void PID_Init(PID_HandleTypeDef *pid, float kp, float ki, float kd)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->setpoint = 0.0f;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->integral_limit = 100.0f;
    pid->output_limit = 300.0f;
}

float PID_Calculate(PID_HandleTypeDef *pid, float input)
{
    float error = pid->setpoint - input;
    float derivative;
    float output;
    
    pid->integral += error;
    // 积分限幅防止饱和
    if(pid->integral > pid->integral_limit) pid->integral = pid->integral_limit;
    else if(pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;
    
    derivative = error - pid->prev_error;
    pid->prev_error = error;
    
    output = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative;
    
    // 输出限幅
    if(output > pid->output_limit) output = pid->output_limit;
    else if(output < -pid->output_limit) output = -pid->output_limit;
    
    return output;
}

void PID_Reset(PID_HandleTypeDef *pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}
