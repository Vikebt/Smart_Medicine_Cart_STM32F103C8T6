#include "cart_state.h"

#define LOAD_THRESHOLD_GRAM       200.0f
#define UNLOAD_THRESHOLD_GRAM      50.0f
#define VISION_TIMEOUT_MS          300U
#define QUARTER_TURN_MS            500U
#define DELIVERY_ACK_MS           1000U
#define HALF_TURN_MS              1000U

static uint8_t prvElapsed(uint32_t ulNow, uint32_t ulStart, uint32_t ulDuration)
{
    return ((uint32_t)(ulNow - ulStart) >= ulDuration) ? 1U : 0U;
}

static void prvEnter(CartStateMachine_t *pSM, CartState_t eState, uint32_t ulNow)
{
    pSM->eState = eState;
    pSM->ulStateEnteredMs = ulNow;
}

void CartSM_Init(CartStateMachine_t *pSM, uint32_t ulNowMs)
{
    if (pSM == 0)
    {
        return;
    }
    pSM->eState = CART_STATE_WAIT_LOAD;
    pSM->eOutboundTurn = CART_TURN_NONE;
    pSM->ulStateEnteredMs = ulNowMs;
    pSM->ucPreviousIntersection = 0U;
}

void CartSM_Step(CartStateMachine_t *pSM,
                 const CartInput_t *pInput,
                 CartOutput_t *pOutput)
{
    uint8_t ucIntersectionRising;

    if ((pSM == 0) || (pInput == 0) || (pOutput == 0))
    {
        return;
    }

    ucIntersectionRising = (uint8_t)(pInput->ucAtIntersection &&
                                     !pSM->ucPreviousIntersection);
    pSM->ucPreviousIntersection = pInput->ucAtIntersection;

    pOutput->ucRequestVision = 0U;
    pOutput->ucRedLamp = 0U;
    pOutput->ucGreenLamp = 0U;

    switch (pSM->eState)
    {
        case CART_STATE_WAIT_LOAD:
            if (pInput->fWeightGram >= LOAD_THRESHOLD_GRAM)
            {
                prvEnter(pSM, CART_STATE_LINE_FOLLOW, pInput->ulNowMs);
            }
            break;

        case CART_STATE_LINE_FOLLOW:
            if (ucIntersectionRising)
            {
                prvEnter(pSM, CART_STATE_WAIT_VISION, pInput->ulNowMs);
                pOutput->ucRequestVision = 1U;
            }
            break;

        case CART_STATE_WAIT_VISION:
            if (pInput->eVisionTurn == CART_TURN_RIGHT)
            {
                pSM->eOutboundTurn = CART_TURN_RIGHT;
                prvEnter(pSM, CART_STATE_TURN_RIGHT, pInput->ulNowMs);
            }
            else if (pInput->eVisionTurn == CART_TURN_LEFT)
            {
                pSM->eOutboundTurn = CART_TURN_LEFT;
                prvEnter(pSM, CART_STATE_TURN_LEFT, pInput->ulNowMs);
            }
            else if (prvElapsed(pInput->ulNowMs, pSM->ulStateEnteredMs,
                                VISION_TIMEOUT_MS))
            {
                prvEnter(pSM, CART_STATE_LINE_FOLLOW, pInput->ulNowMs);
            }
            break;

        case CART_STATE_TURN_RIGHT:
        case CART_STATE_TURN_LEFT:
            if (prvElapsed(pInput->ulNowMs, pSM->ulStateEnteredMs,
                           QUARTER_TURN_MS))
            {
                prvEnter(pSM, CART_STATE_DELIVER, pInput->ulNowMs);
            }
            break;

        case CART_STATE_DELIVER:
            if (pInput->fWeightGram <= UNLOAD_THRESHOLD_GRAM)
            {
                prvEnter(pSM, CART_STATE_DELIVER_ACK, pInput->ulNowMs);
            }
            break;

        case CART_STATE_DELIVER_ACK:
            if (prvElapsed(pInput->ulNowMs, pSM->ulStateEnteredMs,
                           DELIVERY_ACK_MS))
            {
                prvEnter(pSM, CART_STATE_TURN_AROUND, pInput->ulNowMs);
            }
            break;

        case CART_STATE_TURN_AROUND:
            if (prvElapsed(pInput->ulNowMs, pSM->ulStateEnteredMs,
                           HALF_TURN_MS))
            {
                prvEnter(pSM, CART_STATE_RETURN_HOME, pInput->ulNowMs);
            }
            break;

        case CART_STATE_RETURN_HOME:
            if (pInput->ucAtHome)
            {
                pSM->eOutboundTurn = CART_TURN_NONE;
                prvEnter(pSM, CART_STATE_WAIT_LOAD, pInput->ulNowMs);
            }
            else if (ucIntersectionRising)
            {
                if (pSM->eOutboundTurn == CART_TURN_RIGHT)
                {
                    prvEnter(pSM, CART_STATE_RETURN_TURN_LEFT, pInput->ulNowMs);
                }
                else if (pSM->eOutboundTurn == CART_TURN_LEFT)
                {
                    prvEnter(pSM, CART_STATE_RETURN_TURN_RIGHT, pInput->ulNowMs);
                }
            }
            break;

        case CART_STATE_RETURN_TURN_LEFT:
        case CART_STATE_RETURN_TURN_RIGHT:
            if (prvElapsed(pInput->ulNowMs, pSM->ulStateEnteredMs,
                           QUARTER_TURN_MS))
            {
                prvEnter(pSM, CART_STATE_RETURN_HOME, pInput->ulNowMs);
            }
            break;

        default:
            prvEnter(pSM, CART_STATE_WAIT_LOAD, pInput->ulNowMs);
            break;
    }

    pOutput->eState = pSM->eState;
    pOutput->ucUseLineController = 0U;
    pOutput->iLeftSpeed = 0;
    pOutput->iRightSpeed = 0;

    switch (pSM->eState)
    {
        case CART_STATE_LINE_FOLLOW:
        case CART_STATE_RETURN_HOME:
            pOutput->ucUseLineController = 1U;
            break;
        case CART_STATE_TURN_RIGHT:
        case CART_STATE_TURN_AROUND:
        case CART_STATE_RETURN_TURN_RIGHT:
            pOutput->iLeftSpeed = 300;
            pOutput->iRightSpeed = -300;
            break;
        case CART_STATE_TURN_LEFT:
        case CART_STATE_RETURN_TURN_LEFT:
            pOutput->iLeftSpeed = -300;
            pOutput->iRightSpeed = 300;
            break;
        case CART_STATE_DELIVER:
            pOutput->ucRedLamp = 1U;
            break;
        case CART_STATE_DELIVER_ACK:
            pOutput->ucGreenLamp = 1U;
            break;
        default:
            break;
    }
}
