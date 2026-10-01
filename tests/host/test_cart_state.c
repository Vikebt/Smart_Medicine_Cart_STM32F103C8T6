#include "cart_state.h"

#include <assert.h>
#include <string.h>

static CartOutput_t step(CartStateMachine_t *sm, uint32_t now, float weight,
                         uint8_t intersection, uint8_t home, CartTurn_t turn)
{
    CartInput_t in;
    CartOutput_t out;
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.ulNowMs = now;
    in.fWeightGram = weight;
    in.ucAtIntersection = intersection;
    in.ucAtHome = home;
    in.eVisionTurn = turn;
    CartSM_Step(sm, &in, &out);
    return out;
}

int main(void)
{
    CartStateMachine_t sm;
    CartOutput_t out;

    CartSM_Init(&sm, 0U);
    out = step(&sm, 0U, 250.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_LINE_FOLLOW);
    assert(out.ucUseLineController == 1U);

    out = step(&sm, 20U, 250.0f, 1U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_WAIT_VISION);
    assert(out.ucRequestVision == 1U);
    assert(out.iLeftSpeed == 0 && out.iRightSpeed == 0);

    out = step(&sm, 40U, 250.0f, 1U, 0U, CART_TURN_RIGHT);
    assert(out.eState == CART_STATE_TURN_RIGHT);
    assert(out.iLeftSpeed == 300 && out.iRightSpeed == -300);

    out = step(&sm, 540U, 250.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_DELIVER && out.ucRedLamp == 1U);

    out = step(&sm, 560U, 20.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_DELIVER_ACK && out.ucGreenLamp == 1U);
    out = step(&sm, 1560U, 20.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_TURN_AROUND);
    out = step(&sm, 2560U, 20.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_RETURN_HOME && out.ucUseLineController == 1U);
    out = step(&sm, 2580U, 20.0f, 0U, 1U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_WAIT_LOAD);

    CartSM_Init(&sm, 0xFFFFFFF0U);
    sm.eState = CART_STATE_TURN_LEFT;
    sm.ulStateEnteredMs = 0xFFFFFFF0U;
    out = step(&sm, 0x000001E4U, 250.0f, 0U, 0U, CART_TURN_NONE);
    assert(out.eState == CART_STATE_DELIVER);
    return 0;
}
