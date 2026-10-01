#ifndef CART_STATE_H
#define CART_STATE_H

#include <stdint.h>

typedef enum
{
    CART_STATE_WAIT_LOAD = 0,
    CART_STATE_LINE_FOLLOW,
    CART_STATE_WAIT_VISION,
    CART_STATE_TURN_RIGHT,
    CART_STATE_TURN_LEFT,
    CART_STATE_DELIVER,
    CART_STATE_DELIVER_ACK,
    CART_STATE_TURN_AROUND,
    CART_STATE_RETURN_HOME,
    CART_STATE_RETURN_TURN_LEFT,
    CART_STATE_RETURN_TURN_RIGHT
} CartState_t;

typedef enum
{
    CART_TURN_NONE = 0,
    CART_TURN_LEFT,
    CART_TURN_RIGHT
} CartTurn_t;

typedef struct
{
    uint32_t ulNowMs;
    float fWeightGram;
    uint8_t ucAtIntersection;
    uint8_t ucAtHome;
    CartTurn_t eVisionTurn;
} CartInput_t;

typedef struct
{
    CartState_t eState;
    int16_t iLeftSpeed;
    int16_t iRightSpeed;
    uint8_t ucUseLineController;
    uint8_t ucRequestVision;
    uint8_t ucRedLamp;
    uint8_t ucGreenLamp;
} CartOutput_t;

typedef struct
{
    CartState_t eState;
    CartTurn_t eOutboundTurn;
    uint32_t ulStateEnteredMs;
    uint8_t ucPreviousIntersection;
} CartStateMachine_t;

void CartSM_Init(CartStateMachine_t *pStateMachine, uint32_t ulNowMs);
void CartSM_Step(CartStateMachine_t *pStateMachine,
                 const CartInput_t *pInput,
                 CartOutput_t *pOutput);

#endif
