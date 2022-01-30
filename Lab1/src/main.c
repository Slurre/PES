#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "bsp.h"
#include "nrf_atfifo.h"
#include "app_button.h" 
#include "app_timer.h"
#include "app_error.h"
#include "nrf_drv_clock.h"
#include "led_softblink.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define BUTTON_DEBOUNCE_DELAY       50
#define TIMEOUT_DELAY               APP_TIMER_TICKS(10000)
#define SOFTBLINK_TIME              APP_TIMER_TICKS(100)

/* Function pointer primitive */ 
typedef void (*state_func_t)( void );

struct _state 
{
    uint8_t id;
    state_func_t Enter;
    state_func_t Do;
    state_func_t Exit;
    uint32_t delay_ms;
};
typedef struct _state state_t; 

enum _event 
{
    b1_evt = 0,
    b2_evt = 1,
    b3_evt = 2,
    timeout_evt = 3,
    no_evt = 4
};
typedef enum _event event_t; 

/* !!!! PART 2 & 3 */
/* Define FIFO */
NRF_ATFIFO_DEF(event_fifo, event_t, 10);

/* Define Timer */
APP_TIMER_DEF(timeout_timer);

/* For clockwise/counter-clockwise behaviour */
static char get_next_led(bool);

/* Button pushes adds corresponding event to FIFO */
static void button_handler(uint8_t pin_num, uint8_t btn_action)
{
    /* !!!! PART 2 !!!! */
    event_t evt;

    if(btn_action == APP_BUTTON_PUSH){
        switch (pin_num)
        {
        case BUTTON_1:
            evt = b1_evt;
            nrf_atfifo_alloc_put(event_fifo, &evt, sizeof(event_t), NULL);
            break;
        
        case BUTTON_2:
            evt = b2_evt;
            nrf_atfifo_alloc_put(event_fifo, &evt, sizeof(event_t), NULL);
            break;

        case BUTTON_3:
            evt = b3_evt;
            nrf_atfifo_alloc_put(event_fifo, &evt, sizeof(event_t), NULL);
            break;
        
        default:
            break;
        }
    }

    return;
}

static const app_button_cfg_t p_buttons[] = {
    {BUTTON_1, APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_PULLUP, button_handler},
    {BUTTON_2, APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_PULLUP, button_handler},
    {BUTTON_3, APP_BUTTON_ACTIVE_LOW, NRF_GPIO_PIN_PULLUP, button_handler}
};

/* Puts timeout event in FIFO */
static void timeout_handler(void * p_context)
{
    /* !!!! PART 3 !!!! */
    event_t evt = timeout_evt;
    nrf_atfifo_alloc_put(event_fifo, &evt, sizeof(event_t), NULL);
}

void init_board(void)
{
    /* Initialize the low frequency clock used by APP_TIMER */
    uint32_t err_code;
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);

    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("Logging initialized.");

    /* Initialize the event fifo */
    /* !!!! PART 2 !!!! */
    NRF_ATFIFO_INIT(event_fifo);

    /* Initialize the timer module */ 
    /* !!!! PART 3 !!!! */
    app_timer_init();
    app_timer_create(&timeout_timer, APP_TIMER_MODE_SINGLE_SHOT, timeout_handler);

    /* Initialize the LEDs */
    bsp_board_init(BSP_INIT_LEDS);

    /* Setup button interrupt handler */
    /* !!!! PART 2 !!!! */
    app_button_init(p_buttons, ARRAY_SIZE(p_buttons), BUTTON_DEBOUNCE_DELAY);
    app_button_enable();

    /* Initialize fading LED driver */ 
    /* !!!! PART 3 !!!! */
    led_sb_init_params_t led_sb_init_param = LED_SB_INIT_DEFAULT_PARAMS(LEDS_MASK);
    led_sb_init_param.duty_cycle_step = 1; // for slower fading
    err_code = led_softblink_init(&led_sb_init_param);
    APP_ERROR_CHECK(err_code);

    /* Default 'breaks' at min/max vals are too long */
    led_softblink_on_time_set(SOFTBLINK_TIME);
    led_softblink_off_time_set(SOFTBLINK_TIME);

    NRF_LOG_INFO("init_board() finished");
}

event_t get_event(void)
{
    /* !!!! PART 2 !!!! */
    event_t evt = no_evt;
    nrf_atfifo_get_free(event_fifo, &evt, sizeof(event_t), NULL);
    return evt;
}

static void do_state_0(void)
{
    char next_led;
    bsp_board_leds_off();
    next_led = get_next_led(false);
    bsp_board_led_on(next_led);
}

static void do_state_1(void)
{
    for(int i = 0; i < LEDS_NUMBER; i++){
        bsp_board_led_invert(i);
    }
}

static void do_state_2(void)
{
    char next_led;
    bsp_board_leds_off();
    next_led = get_next_led(true);
    bsp_board_led_on(next_led);
}

static void do_state_3(void)
{
    return;  
}

static void enter_state_3(void)
{
    ret_code_t err_code;
    err_code = app_timer_start(timeout_timer, TIMEOUT_DELAY, NULL);
    APP_ERROR_CHECK(err_code);
    err_code = led_softblink_start(LEDS_MASK);
    APP_ERROR_CHECK(err_code);
}

static void exit_state_3(void)
{
    ret_code_t err_code;
    err_code = led_softblink_stop();
    APP_ERROR_CHECK(err_code);
}

static char get_next_led(bool reverse){
    /* !!!! PART 1 !!!! */
    static char order[] = {0, 1, 3, 2};
    static unsigned int i = 0;
    char idx = order[i];

    if (reverse)
        i = (i - 1) % 4;
    else 
        i = (i + 1) % 4;

    return idx;
}   



/* !!!! PART 1 !!!! */
const state_t state0 = {
    0, 
    bsp_board_leds_off,
    do_state_0,
    bsp_board_leds_off, 
    150
};

const state_t state1 = {
    1, 
    bsp_board_leds_off,
    do_state_1,
    bsp_board_leds_off, 
    200
};

const state_t state2 = {
    2, 
    bsp_board_leds_off,
    do_state_2,
    bsp_board_leds_off, 
    100
};

const state_t state3 = {
    3, 
    enter_state_3,
    do_state_3,
    exit_state_3, 
    100
};


/* !!!! PART 2 !!!! */
const state_t state_table[4][5] = {
//*  STATE       B1          B2          B3          TIMOUT      NO_EVT */
{/*   S0 */      state2,     state1,     state3,     state0,     state0},
{/*   S1 */      state0,     state2,     state3,     state1,     state1},
{/*   S2 */      state1,     state0,     state3,     state2,     state2},
{/*   S3 */      state3,     state3,     state3,     state0,     state3}
};

/* !!!! FOR ALL PARTS !!!! */
int main(void)
{
    NRF_LOG_INFO("In main");
    init_board();

    state_t current_state = state0;
    event_t evt = no_evt;

    for(;;)
    {
        current_state.Enter();
        evt = get_event();

        while(current_state.id == state_table[current_state.id][evt].id)
        {
            current_state.Do();
            nrf_delay_ms(current_state.delay_ms);
            evt = get_event();
            NRF_LOG_FLUSH();
        }
        current_state.Exit();
        current_state = state_table[current_state.id][evt];
    }
    
}
