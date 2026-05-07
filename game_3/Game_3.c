#include "Game_3.h"

#include "main.h"
#include "adc.h"
#include "gpio.h"
#include "LCD.h"
#include "Joystick.h"
#include "Utils.h"
#include "Buzzer.h"
#include "PWM.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================
 * Shared objects from main.c
 * ========================================================= */
extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern volatile uint32_t g_tim6_ticks;
extern volatile uint8_t g_tim6_led_flag;
extern ADC_HandleTypeDef hadc1;
extern Buzzer_cfg_t buzzer_cfg;
extern PWM_cfg_t pwm_cfg;

/* =========================================================
 * LCD pixel function
 * ========================================================= */
#ifndef SELLING_LCD_PIXEL
#define SELLING_LCD_PIXEL LCD_Set_Pixel
#endif

/* =========================================================
 * Hardware mapping
 * ========================================================= */
#define SELLING_TRADE_BUTTON_PORT         BTN2_GPIO_Port
#define SELLING_TRADE_BUTTON_PIN          BTN2_Pin

#define SELLING_JOY_SW_PORT               BTN3_GPIO_Port
#define SELLING_JOY_SW_PIN                BTN3_Pin

#define SELLING_ALERT_LED_PORT            GPIOB
#define SELLING_ALERT_LED_PIN             GPIO_PIN_6

/* =========================================================
 * Screen / timing
 * ========================================================= */
#define SELLING_W                         240
#define SELLING_H                         240

#define SELLING_MAX_CUSTOMERS             3
#define SELLING_GAME_TIME_SECONDS         120
#define SELLING_MAX_PATIENCE              25

#define SELLING_PLAYER_MIN_X              40
#define SELLING_PLAYER_MAX_X              200
#define SELLING_PLAYER_MOVE_STEP          24

#define SELLING_UPDATE_INTERVAL_MS        4
#define SELLING_RENDER_INTERVAL_MS        28
#define SELLING_PATIENCE_INTERVAL_MS      1000
#define SELLING_SECOND_INTERVAL_MS        1000

#define SELLING_TRADE_DEBOUNCE_MS         1
#define SELLING_DISH_DEBOUNCE_MS          70
#define SELLING_BUTTON_EVENT_GAP_MS       1

#define SELLING_ALERT_FLASH_DIV           30
#define SELLING_ALERT_FAST_DIV            12
#define SELLING_ANGRY_LEAVE_MS            2000

#define SELLING_LOADING_TOTAL_MS          50
#define SELLING_LOADING_STEP_MS           4

#define SELLING_COUNTDOWN_MS              520
#define SELLING_START_FLASH_MS            720

#define SELLING_LOOP_DELAY_MS             1

#define SELLING_SELECT_DISTANCE_TH        22

#define SELLING_WRONG_TIME_PENALTY_S      2
#define SELLING_WRONG_FLASH_TOTAL_MS      1500
#define SELLING_WRONG_FLASH_STEP_MS       90

#define SELLING_RESULT_MENU_NAV_MS        180
#define SELLING_RESULT_ENTER_DELAY_MS     180



/* =========================================================
 * Joystick anti-drift
 * ========================================================= */
#define SELLING_JOY_SAMPLE_COUNT          3
#define SELLING_JOY_CALIB_SAMPLES         18
#define SELLING_JOY_ENTER_DEADZONE        320
#define SELLING_JOY_EXIT_DEADZONE         120

/* =========================================================
 * Prices
 * ========================================================= */
#define SELLING_PRICE_SOUP                8
#define SELLING_PRICE_BURGER              10
#define SELLING_PRICE_CHIPS               12

/* =========================================================
 * Customer spawn timing
 * ========================================================= */
#define SELLING_RESPAWN_MIN_MS            1200
#define SELLING_RESPAWN_MAX_MS            4500
#define SELLING_FINISH_RESPAWN_MIN_MS     800
#define SELLING_FINISH_RESPAWN_MAX_MS     2500

#define SELLING_INIT_SPAWN0_MIN_MS        300
#define SELLING_INIT_SPAWN0_MAX_MS        1200
#define SELLING_INIT_SPAWN1_MIN_MS        2200
#define SELLING_INIT_SPAWN1_MAX_MS        3800
#define SELLING_INIT_SPAWN2_MIN_MS        4200
#define SELLING_INIT_SPAWN2_MAX_MS        6200

/* =========================================================
 * Colors 
 * ========================================================= */
#define C_BLACK             0
#define C_WHITE             1

#define C_WALL_CREAM        14
#define C_WALL_CREAM_2      4
#define C_WALL_LINE         11
#define C_WALL_SHADOW       4

#define C_AWNING_RED        9
#define C_AWNING_RED_DARK   3
#define C_AWNING_CREAM      14

#define C_SIGN_WOOD         9
#define C_SIGN_WOOD_DARK    3
#define C_SIGN_GOLD         10

#define C_SHOP_SIGN_MAIN    9
#define C_SHOP_SIGN_DARK    3
#define C_SHOP_SIGN_LIGHT   11
#define C_SHOP_SIGN_TEXT    1

#define C_DESK_SIGN_MAIN    3
#define C_DESK_SIGN_DARK    0
#define C_DESK_SIGN_LIGHT   11

#define C_WOOD_LIGHT        4
#define C_WOOD_TOP          3
#define C_WOOD_MID          11
#define C_WOOD_DARK         0
#define C_WOOD_LINE         14
#define C_POST_SHADE        3
#define C_DISH_TAG_FILL     15

#define C_PANEL_BG          4
#define C_PANEL_INNER       14
#define C_PANEL_EDGE        3
#define C_PANEL_SHADOW      11
#define C_PANEL_BAR         9

#define C_FEEDBACK_BG       11
#define C_FEEDBACK_BORDER   3
#define C_FEEDBACK_INNER    9

#define C_MENU_BOARD        9
#define C_MENU_BOARD_2      3
#define C_MENU_FRAME        10

#define C_HUD_FILL          11
#define C_HUD_EDGE          3
#define C_HUD_TOP           9
#define C_HUD_ICON          10

#define C_ACCENT_GREEN      13
#define C_ACCENT_RED        10
#define C_ACCENT_BLUE       9
#define C_ACCENT_GOLD       10
#define C_ACCENT_SKY        11

#define C_PATIENCE_BLUE     13
#define C_PATIENCE_RED      10
#define C_PATIENCE_BG       4

#define C_FLOOR_A           14
#define C_FLOOR_B           4
#define C_FLOOR_LINE        11

#define C_GREY              3
#define C_GREY_LIGHT        14

#define C_SKIN              15
#define C_SKIN_DARK         11
#define C_HAIR              3
#define C_HAIR_BROWN        0
#define C_HAIR_BLONDE       10
#define C_BEARD             3

#define C_HAT               9
#define C_HAT_DARK          3
#define C_FISHERMAN_COAT    9
#define C_FISHERMAN_PANTS   3
#define C_FISHERMAN_BOOT    0
#define C_NET_COLOR         11

#define C_CUST_CLOTH_1      9
#define C_CUST_CLOTH_1B     3
#define C_CUST_CLOTH_2      13
#define C_CUST_CLOTH_2B     3
#define C_CUST_CLOTH_3      10
#define C_CUST_CLOTH_3B     3

#define C_SOUP_BOWL         14
#define C_SOUP_BOWL_DARK    3
#define C_SOUP_SURFACE      10
#define C_SOUP_SURFACE_2    15
#define C_SOUP_STEAM        1
#define C_SOUP_GARNISH      13

#define C_BURGER_BUN_TOP    10
#define C_BURGER_BUN_BOT    10
#define C_BURGER_PATTY      3
#define C_BURGER_CHEESE     15
#define C_BURGER_LETTUCE    13
#define C_BURGER_TOMATO     10
#define C_BURGER_SESAME     1

#define C_CHIPS_BOX         10
#define C_CHIPS_BOX_DARK    3
#define C_CHIPS_FRIES       15

#define C_FISH_BODY         11
#define C_FISH_BODY_DARK    3
#define C_FISH_FIN          9
#define C_FISH_EYE          0

#define C_PLATE             14
#define C_PLATE_INNER       1

#define C_LOAD_BG           14
#define C_LOAD_BAR_BG       4
#define C_LOAD_BAR_FG       9
#define C_LOAD_BAR_FG2      3

#define C_DANGER            10
#define C_WARN_BG           15
#define C_WARN_EDGE         10
#define C_WARN_FILL         14

#define C_HIGHLIGHT         13

/* =========================================================
 * Layout
 * ========================================================= */
#define HUD_L_X         8
#define HUD_L_Y         8
#define HUD_L_W         108
#define HUD_L_H         30

#define HUD_R_X         124
#define HUD_R_Y         8
#define HUD_R_W         108
#define HUD_R_H         30

#define CUST0_X         52
#define CUST1_X         120
#define CUST2_X         188
#define CUST_Y          112

#define CARD0_X         10
#define CARD1_X         86
#define CARD2_X         162
#define CARD_Y          44
#define CARD_W          68
#define CARD_H          42

#define PATIENCE0_X     18
#define PATIENCE1_X     94
#define PATIENCE2_X     170
#define PATIENCE_Y      88
#define PATIENCE_W      52
#define PATIENCE_H      8

#define COUNTER_Y       136

#define DISH_L_X        52
#define DISH_C_X        120
#define DISH_R_X        188
#define DISH_Y          176

#define DISH_FRAME_Y    154
#define DISH_FRAME_W    52
#define DISH_FRAME_H    56

#define FEEDBACK_X      20
#define FEEDBACK_Y      220
#define FEEDBACK_W      200
#define FEEDBACK_H      18

/* =========================================================
 * Data types
 * ========================================================= */
typedef enum {
    SELLING_DISH_COD_SOUP = 0,
    SELLING_DISH_COD_BURGER,
    SELLING_DISH_FISH_CHIPS,
    SELLING_DISH_COUNT
} SellingDishType;

typedef struct {
    uint8_t count[SELLING_DISH_COUNT];
    uint8_t totalRemaining;
    uint8_t patience;

    uint8_t active;
    uint8_t angryLeaving;
    uint8_t waitingSpawn;

    uint32_t angryStartMs;
    uint32_t spawnAtMs;

    int16_t x;
    int16_t y;
} SellingCustomer;

typedef struct {
    int16_t x;
    uint8_t currentTarget;
    uint8_t targetLocked;
    SellingDishType selectedDish;
} SellingPlayer;

typedef struct {
    uint16_t gold;
    uint16_t customersServed;
    int16_t remainingTime;
    uint8_t running;
    uint8_t finished;

    uint8_t rewardVisible;
    uint8_t rewardValue;
    uint32_t rewardStartMs;
    int16_t rewardY;

    char feedbackText[32];

    SellingCustomer customers[SELLING_MAX_CUSTOMERS];
    SellingPlayer player;
} SellingPhaseGame;

/* =========================================================
 * State
 * ========================================================= */
static SellingPhaseGame selling_game;

static uint32_t selling_lastUpdateMs = 0;
static uint32_t selling_lastRenderMs = 0;
static uint32_t selling_lastSecondMs = 0;
static uint32_t selling_lastPatienceMs = 0;
static uint32_t selling_lastTradeMs = 0;
static uint32_t selling_lastDishChangeMs = 0;
static uint32_t selling_finishMs = 0;

static uint16_t selling_center_x = 2048;
static int8_t selling_x_dir_state = 0;

static const int16_t selling_customerX[SELLING_MAX_CUSTOMERS] = { CUST0_X, CUST1_X, CUST2_X };
static const int16_t selling_customerY[SELLING_MAX_CUSTOMERS] = { CUST_Y, CUST_Y, CUST_Y };

static uint8_t  selling_penaltyActive = 0;
static uint8_t  selling_penaltyTargetIdx = 0;
static uint32_t selling_penaltyStartMs = 0;
static uint32_t selling_penaltyEndMs = 0;

static uint8_t  selling_wrongAlertActive = 0;
static uint32_t selling_wrongAlertEndMs = 0;

static uint8_t  selling_ledAlertEnabled = 0;
static uint8_t  selling_ledState = 0;
static uint8_t  selling_ledBlinkDivider = 0;
static uint8_t  selling_ledBlinkTargetDiv = SELLING_ALERT_FLASH_DIV;

static uint32_t selling_angryLastToggleMs = 0;

static uint8_t  selling_gameOverMelodyPlayed = 0;
static uint8_t  selling_resultMelodyPlayed = 0;
static uint8_t  selling_resultSelection = 0;
static uint32_t selling_lastMenuNavMs = 0;
static uint32_t selling_resultEnterMs = 0;

/* =========================================================
 * Forward declarations
 * ========================================================= */
static void selling_draw_dish_station(int cx, int cy, SellingDishType dish, const char *label, uint16_t tabColor);

/* =========================================================
 * Basic drawing helpers
 * ========================================================= */
static void selling_put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= SELLING_W || y < 0 || y >= SELLING_H) return;
    SELLING_LCD_PIXEL(x, y, color);
}

static void selling_hline(int x, int y, int w, uint16_t color)
{
    for (int i = 0; i < w; i++) selling_put_pixel(x + i, y, color);
}

static void selling_vline(int x, int y, int h, uint16_t color)
{
    for (int i = 0; i < h; i++) selling_put_pixel(x, y + i, color);
}

static void selling_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            selling_put_pixel(x + xx, y + yy, color);
        }
    }
}

static void selling_draw_rect(int x, int y, int w, int h, uint16_t color)
{
    selling_hline(x, y, w, color);
    selling_hline(x, y + h - 1, w, color);
    selling_vline(x, y, h, color);
    selling_vline(x + w - 1, y, h, color);
}

static void selling_draw_rect_thick(int x, int y, int w, int h, int t, uint16_t color)
{
    for (int k = 0; k < t; k++) {
        selling_draw_rect(x + k, y + k, w - 2 * k, h - 2 * k, color);
    }
}

static void selling_fill_circle(int cx, int cy, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                selling_put_pixel(cx + x, cy + y, color);
            }
        }
    }
}

static void selling_draw_round_panel(int x, int y, int w, int h, uint16_t fill, uint16_t border)
{
    selling_fill_rect(x + 2, y, w - 4, h, fill);
    selling_fill_rect(x, y + 2, w, h - 4, fill);

    selling_fill_circle(x + 2, y + 2, 2, fill);
    selling_fill_circle(x + w - 3, y + 2, 2, fill);
    selling_fill_circle(x + 2, y + h - 3, 2, fill);
    selling_fill_circle(x + w - 3, y + h - 3, 2, fill);

    selling_draw_rect(x, y, w, h, border);
}

static void selling_draw_pixel_frame(int x, int y, int w, int h, uint16_t fill, uint16_t border, uint16_t inner)
{
    selling_fill_rect(x + 2, y + 2, w, h, C_PANEL_SHADOW);
    selling_fill_rect(x, y, w, h, border);
    selling_fill_rect(x + 2, y + 2, w - 4, h - 4, fill);
    if (w > 8 && h > 8) {
        selling_draw_rect(x + 3, y + 3, w - 6, h - 6, inner);
    }
}

static void selling_draw_star_spark(int cx, int cy, uint16_t color)
{
    selling_hline(cx - 2, cy, 5, color);
    selling_vline(cx, cy - 2, 5, color);
    selling_put_pixel(cx - 1, cy - 1, color);
    selling_put_pixel(cx + 1, cy - 1, color);
    selling_put_pixel(cx - 1, cy + 1, color);
    selling_put_pixel(cx + 1, cy + 1, color);
}

/* =========================================================
 * Text wrapper
 * ========================================================= */
static void selling_print_black_impl(char *text, int x, int y, int mode, int size)
{
    LCD_printString(text, x, y, mode, size);
}

#undef LCD_printString
#define LCD_printString(str, x, y, mode, size) \
    selling_print_black_impl((char *)(str), (x), (y), (mode), (size))

/* =========================================================
 * Utility
 * ========================================================= */
static uint32_t selling_now_ms(void)
{
    return HAL_GetTick();
}

static int selling_abs_int(int v)
{
    return (v < 0) ? -v : v;
}

static uint8_t selling_random_range(uint8_t min, uint8_t max)
{
    return (uint8_t)(min + (rand() % (max - min + 1)));
}

static uint32_t selling_random_range_u32(uint32_t min, uint32_t max)
{
    if (max <= min) return min;
    return min + (uint32_t)(rand() % (max - min + 1));
}

static const char* selling_dish_name(SellingDishType dish)
{
    switch (dish) {
    case SELLING_DISH_COD_SOUP:   return "Soup";
    case SELLING_DISH_COD_BURGER: return "Burger";
    case SELLING_DISH_FISH_CHIPS: return "Chips";
    default:                      return "Unknown";
    }
}

static const char* selling_dish_short(SellingDishType dish)
{
    switch (dish) {
    case SELLING_DISH_COD_SOUP:   return "Su";
    case SELLING_DISH_COD_BURGER: return "Bu";
    case SELLING_DISH_FISH_CHIPS: return "FC";
    default:                      return "--";
    }
}

static uint8_t selling_dish_price(SellingDishType dish)
{
    switch (dish) {
    case SELLING_DISH_COD_SOUP:   return SELLING_PRICE_SOUP;
    case SELLING_DISH_COD_BURGER: return SELLING_PRICE_BURGER;
    case SELLING_DISH_FISH_CHIPS: return SELLING_PRICE_CHIPS;
    default:                      return 0;
    }
}

static void selling_format_time(int totalSec, char *buf, int bufSize)
{
    int min = totalSec / 60;
    int sec = totalSec % 60;
    snprintf(buf, bufSize, "%d:%02d", min, sec);
}

static uint8_t selling_customer_total_remaining(const SellingCustomer *c)
{
    uint8_t sum = 0;
    for (int i = 0; i < SELLING_DISH_COUNT; i++) sum += c->count[i];
    return sum;
}

static void selling_draw_right_aligned_text(const char *txt, int rightX, int y, int size)
{
    int len = (int)strlen(txt);
    int charW = (size == 1) ? 6 : ((size == 2) ? 8 : 12);
    int textW = len * charW;
    int x = rightX - textW;
    LCD_printString((char *)txt, x, y, 1, size);
}

static void selling_draw_centered_text_1(const char *txt, int x, int y, int w)
{
    int len = (int)strlen(txt);
    int textW = len * 6;
    int tx = x + (w - textW) / 2;
    if (tx < x + 3) tx = x + 3;
    LCD_printString((char *)txt, tx, y, 1, 1);
}

static void selling_draw_centered_text_2(const char *txt, int x, int y, int w)
{
    int len = (int)strlen(txt);
    int textW = len * 12;
    int tx = x + (w - textW) / 2;
    if (tx < x + 3) tx = x + 3;
    LCD_printString((char *)txt, tx, y, 1, 2);
}

/* =========================================================
 * Sound helpers
 * ========================================================= */
static void selling_beep(uint32_t freq, uint16_t dur_ms, uint8_t duty)
{
    buzzer_tone(&buzzer_cfg, freq, duty);
    HAL_Delay(dur_ms);
    buzzer_off(&buzzer_cfg);
}

static void selling_rest(uint16_t dur_ms)
{
    buzzer_off(&buzzer_cfg);
    HAL_Delay(dur_ms);
}

static void selling_play_loading_tick_sfx(uint8_t stage)
{
    /* Short rising arpeggio ticks: richer than one beep, but still fast. */
    switch (stage) {
    case 0:
        selling_beep(659, 18, 32);
        selling_rest(4);
        selling_beep(784, 20, 34);
        break;
    case 1:
        selling_beep(784, 18, 32);
        selling_rest(4);
        selling_beep(988, 22, 34);
        break;
    case 2:
        selling_beep(880, 18, 32);
        selling_rest(4);
        selling_beep(1175, 24, 35);
        break;
    case 3:
        selling_beep(988, 18, 32);
        selling_rest(4);
        selling_beep(1319, 26, 36);
        break;
    default:
        selling_beep(1319, 28, 36);
        break;
    }
    selling_rest(4);
}

static void selling_play_loading_finish_melody(void)
{
    /* Fast finish melody: C-E-G-C style, less monotonous and not too long. */
    selling_beep(1047, 36, 36);
    selling_rest(6);
    selling_beep(1319, 36, 36);
    selling_rest(6);
    selling_beep(1568, 44, 38);
    selling_rest(6);
    selling_beep(2093, 64, 40);
    selling_rest(10);
}

static void selling_play_countdown_beep(uint8_t n)
{
    if (n == 3) selling_beep(740, 100, 40);
    else if (n == 2) selling_beep(880, 105, 40);
    else selling_beep(1047, 120, 40);
    selling_rest(18);
}

static void selling_play_go_sfx(void)
{
    selling_beep(1319, 75, 40);
    selling_rest(18);
    selling_beep(1568, 120, 40);
    selling_rest(18);
    selling_beep(1760, 145, 40);
    selling_rest(28);
}

static void selling_play_wrong_sfx(void)
{
    /* Wrong dish alarm: short, fast and sharp, easy to distinguish. */
    selling_beep(1760, 42, 45);
    selling_rest(10);
    selling_beep(1397, 42, 45);
    selling_rest(10);
    selling_beep(1760, 42, 45);
    selling_rest(10);
    selling_beep(1175, 85, 45);
    selling_rest(12);
}

static void selling_play_angry_sfx(void)
{
    /* Angry customer alarm: lower descending warning melody. */
    selling_beep(784, 70, 45);
    selling_rest(18);
    selling_beep(659, 75, 45);
    selling_rest(18);
    selling_beep(523, 105, 45);
    selling_rest(18);
}

static void selling_play_gameover_melody(void)
{
  
    selling_beep(1568, 80, 38);
    selling_rest(16);
    selling_beep(1319, 90, 38);
    selling_rest(16);
    selling_beep(1047, 105, 38);
    selling_rest(18);
    selling_beep(784, 135, 38);
    selling_rest(24);
    selling_beep(659, 170, 38);
    selling_rest(30);
}

static void selling_play_result_melody(void)
{
    
    selling_beep(784, 70, 36);
    selling_rest(12);
    selling_beep(988, 70, 36);
    selling_rest(12);
    selling_beep(1175, 80, 36);
    selling_rest(12);
    selling_beep(1568, 95, 38);
    selling_rest(18);
    selling_beep(1319, 80, 36);
    selling_rest(12);
    selling_beep(1568, 130, 40);
    selling_rest(24);
    selling_beep(2093, 170, 40);
    selling_rest(28);
}

/* =========================================================
 * Result text
 * ========================================================= */
static const char* selling_get_result_message(uint16_t served, uint16_t gold)
{
    if (served >= 18 || gold >= 160) return "Legendary seller!";
    if (served >= 14 || gold >= 120) return "Shop star!";
    if (served >= 10 || gold >= 85)  return "Great catch today!";
    if (served >= 6  || gold >= 50)  return "Nice work!";
    if (served >= 3  || gold >= 25)  return "Keep practicing!";
    return "A quiet market day.";
}

/* =========================================================
 * ADC / input
 * ========================================================= */
static uint16_t selling_read_adc_channel(uint32_t channel, uint32_t sampling_time)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = sampling_time;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);

    return (uint16_t)HAL_ADC_GetValue(&hadc1);
}

static uint16_t selling_read_vrx_avg(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < SELLING_JOY_SAMPLE_COUNT; i++) {
        sum += selling_read_adc_channel(joystick_cfg.x_channel, joystick_cfg.sampling_time);
    }
    return (uint16_t)(sum / SELLING_JOY_SAMPLE_COUNT);
}

static void selling_calibrate_joystick_center(void)
{
    uint32_t sum = 0;

    HAL_Delay(120);

    for (int i = 0; i < SELLING_JOY_CALIB_SAMPLES; i++) {
        sum += selling_read_vrx_avg();
        HAL_Delay(3);
    }

    selling_center_x = (uint16_t)(sum / SELLING_JOY_CALIB_SAMPLES);
    selling_x_dir_state = 0;
}

static int8_t selling_get_joy_x_dir(uint16_t vrx)
{
    int diff = (int)vrx - (int)selling_center_x;

    if (diff > -SELLING_JOY_EXIT_DEADZONE && diff < SELLING_JOY_EXIT_DEADZONE) {
        selling_x_dir_state = 0;
        return 0;
    }

    if (diff >= SELLING_JOY_ENTER_DEADZONE) selling_x_dir_state = 1;
    else if (diff <= -SELLING_JOY_ENTER_DEADZONE) selling_x_dir_state = -1;

    return selling_x_dir_state;
}

static uint8_t selling_read_trade_button_level(void)
{
    return (HAL_GPIO_ReadPin(SELLING_TRADE_BUTTON_PORT, SELLING_TRADE_BUTTON_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

static uint8_t selling_read_joy_sw_level(void)
{
    return (HAL_GPIO_ReadPin(SELLING_JOY_SW_PORT, SELLING_JOY_SW_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

static uint8_t selling_trade_button_pressed_event(uint32_t nowMs)
{
    static uint8_t prevPressed = 0;
    static uint32_t lastEventMs = 0;
    uint8_t pressed = selling_read_trade_button_level();

    if (pressed && !prevPressed) {
        prevPressed = 1;
        if ((nowMs - lastEventMs) >= SELLING_BUTTON_EVENT_GAP_MS) {
            lastEventMs = nowMs;
            return 1U;
        }
    }

    if (!pressed) prevPressed = 0;
    return 0U;
}

static uint8_t selling_joy_sw_pressed_event(uint32_t nowMs)
{
    static uint8_t prevPressed = 0;
    static uint32_t lastEventMs = 0;
    uint8_t pressed = selling_read_joy_sw_level();

    if (pressed && !prevPressed) {
        prevPressed = 1;
        if ((nowMs - lastEventMs) >= SELLING_BUTTON_EVENT_GAP_MS) {
            lastEventMs = nowMs;
            return 1U;
        }
    }

    if (!pressed) prevPressed = 0;
    return 0U;
}

/* =========================================================
 * LED alarm logic based on Unit_3_4_FSM_Game PWM style
 * PB6 / D10 = TIM4_CH1, controlled directly by PWM_SetDuty().
 * Different alarm events use different blink frequencies:
 *   - Wrong dish: fast flash
 *   - Angry customer: slow flash
 * ========================================================= */
#define SELLING_LED_PWM_OFF_DUTY          0
#define SELLING_LED_PWM_ON_DUTY           100
#define SELLING_LED_WRONG_INTERVAL_MS     45
#define SELLING_LED_ANGRY_INTERVAL_MS     260

typedef enum {
    SELLING_LED_ALERT_NONE = 0,
    SELLING_LED_ALERT_WRONG,
    SELLING_LED_ALERT_ANGRY
} SellingLedAlertType;

static SellingLedAlertType selling_ledAlertType = SELLING_LED_ALERT_NONE;
static uint32_t selling_ledLastToggleMs = 0;
static uint32_t selling_ledCurrentIntervalMs = SELLING_LED_ANGRY_INTERVAL_MS;

static void selling_led_pwm_off(void)
{
    PWM_SetDuty(&pwm_cfg, SELLING_LED_PWM_OFF_DUTY);
}

static void selling_led_pwm_on(void)
{
    PWM_SetDuty(&pwm_cfg, SELLING_LED_PWM_ON_DUTY);
}

static void selling_led_alert_start_type(SellingLedAlertType type)
{
    selling_ledAlertEnabled = 1;
    selling_ledAlertType = type;
    selling_ledBlinkDivider = 0;
    selling_ledState = 1;
    selling_ledLastToggleMs = selling_now_ms();
    g_tim6_led_flag = 0;

    if (type == SELLING_LED_ALERT_WRONG) {
        selling_ledBlinkTargetDiv = SELLING_ALERT_FAST_DIV;
        selling_ledCurrentIntervalMs = SELLING_LED_WRONG_INTERVAL_MS;
    } else if (type == SELLING_LED_ALERT_ANGRY) {
        selling_ledBlinkTargetDiv = SELLING_ALERT_FLASH_DIV;
        selling_ledCurrentIntervalMs = SELLING_LED_ANGRY_INTERVAL_MS;
    } else {
        selling_ledBlinkTargetDiv = SELLING_ALERT_FLASH_DIV;
        selling_ledCurrentIntervalMs = SELLING_LED_ANGRY_INTERVAL_MS;
    }

    selling_led_pwm_on();
}

static void selling_led_alert_start(uint8_t div)
{
    if (div <= SELLING_ALERT_FAST_DIV) {
        selling_led_alert_start_type(SELLING_LED_ALERT_WRONG);
    } else {
        selling_led_alert_start_type(SELLING_LED_ALERT_ANGRY);
    }
}

static void selling_led_alert_stop(void)
{
    selling_ledAlertEnabled = 0;
    selling_ledAlertType = SELLING_LED_ALERT_NONE;
    selling_ledBlinkDivider = 0;
    selling_ledState = 0;
    selling_ledLastToggleMs = 0;
    selling_ledCurrentIntervalMs = SELLING_LED_ANGRY_INTERVAL_MS;
    g_tim6_led_flag = 0;

    selling_led_pwm_off();
}

static void selling_led_update_from_timer(void)
{
    uint32_t nowMs;

    if (!selling_ledAlertEnabled) {
        selling_led_pwm_off();
        g_tim6_led_flag = 0;
        return;
    }

    nowMs = selling_now_ms();

    if ((nowMs - selling_ledLastToggleMs) < selling_ledCurrentIntervalMs) {
        return;
    }

    selling_ledLastToggleMs = nowMs;
    selling_ledState = !selling_ledState;

    if (selling_ledState) {
        selling_led_pwm_on();
    } else {
        selling_led_pwm_off();
    }
}

static void selling_led_loading_breath(uint8_t percent)
{
    uint8_t phase;
    uint8_t duty;

    phase = (uint8_t)((percent * 2U) % 100U);

    if (phase <= 50U) {
        duty = (uint8_t)(phase * 2U);
    } else {
        duty = (uint8_t)((100U - phase) * 2U);
    }

    if (duty < 8U) duty = 8U;
    PWM_SetDuty(&pwm_cfg, duty);
}

/* =========================================================
 * Alert outputs
 * ========================================================= */
static uint8_t selling_any_customer_angry(void)
{
    for (uint8_t i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
        if (selling_game.customers[i].angryLeaving) return 1U;
    }
    return 0U;
}

static void selling_buzzer_stop(void)
{
    buzzer_off(&buzzer_cfg);
}

static void selling_start_wrong_alert(uint32_t nowMs)
{
    (void)nowMs;
    selling_wrongAlertActive = 1;
    selling_wrongAlertEndMs = selling_now_ms() + SELLING_WRONG_FLASH_TOTAL_MS;
    selling_led_alert_start_type(SELLING_LED_ALERT_WRONG);
    selling_play_wrong_sfx();
}

static void selling_update_alert_outputs(uint32_t nowMs)
{
    if (selling_wrongAlertActive) {
        selling_led_update_from_timer();

        if (nowMs >= selling_wrongAlertEndMs) {
            selling_wrongAlertActive = 0;
            selling_led_alert_stop();
            selling_buzzer_stop();
        }
        return;
    }

    if (selling_any_customer_angry()) {
        if (!selling_ledAlertEnabled || selling_ledAlertType != SELLING_LED_ALERT_ANGRY) {
            selling_led_alert_start_type(SELLING_LED_ALERT_ANGRY);
        }

        selling_led_update_from_timer();

        if ((nowMs - selling_angryLastToggleMs) >= 900) {
            selling_angryLastToggleMs = nowMs;
            selling_play_angry_sfx();
        }
        return;
    }

    selling_led_alert_stop();
    selling_buzzer_stop();
}

/* =========================================================
 * Customer logic
 * ========================================================= */
static void selling_activate_customer(SellingCustomer *customer, uint8_t idx)
{
    uint8_t total = selling_random_range(1, 3);

    memset(customer->count, 0, sizeof(customer->count));
    for (uint8_t i = 0; i < total; i++) {
        uint8_t dish = selling_random_range(0, SELLING_DISH_COUNT - 1);
        customer->count[dish]++;
    }

    customer->totalRemaining = selling_customer_total_remaining(customer);
    customer->patience = SELLING_MAX_PATIENCE;
    customer->active = 1;
    customer->angryLeaving = 0;
    customer->waitingSpawn = 0;
    customer->angryStartMs = 0;
    customer->spawnAtMs = 0;
    customer->x = selling_customerX[idx];
    customer->y = selling_customerY[idx];
}

static void selling_schedule_spawn(uint8_t idx, uint32_t nowMs, uint32_t minDelayMs, uint32_t maxDelayMs)
{
    SellingCustomer *customer = &selling_game.customers[idx];

    memset(customer->count, 0, sizeof(customer->count));
    customer->totalRemaining = 0;
    customer->patience = SELLING_MAX_PATIENCE;
    customer->active = 0;
    customer->angryLeaving = 0;
    customer->waitingSpawn = 1;
    customer->angryStartMs = 0;
    customer->spawnAtMs = nowMs + selling_random_range_u32(minDelayMs, maxDelayMs);
    customer->x = selling_customerX[idx];
    customer->y = selling_customerY[idx];
}

static void selling_schedule_initial_spawns(uint32_t nowMs)
{
    selling_schedule_spawn(0, nowMs, SELLING_INIT_SPAWN0_MIN_MS, SELLING_INIT_SPAWN0_MAX_MS);
    selling_schedule_spawn(1, nowMs, SELLING_INIT_SPAWN1_MIN_MS, SELLING_INIT_SPAWN1_MAX_MS);
    selling_schedule_spawn(2, nowMs, SELLING_INIT_SPAWN2_MIN_MS, SELLING_INIT_SPAWN2_MAX_MS);
}

static void selling_update_spawn_queue(uint32_t nowMs)
{
    for (uint8_t i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
        SellingCustomer *customer = &selling_game.customers[i];
        if (customer->waitingSpawn && nowMs >= customer->spawnAtMs) {
            selling_activate_customer(customer, i);
        }
    }
}

static uint8_t selling_find_near_customer(void)
{
    int bestDist = 9999;
    uint8_t bestIdx = 255;

    for (uint8_t i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
        if (!selling_game.customers[i].active) continue;
        if (selling_game.customers[i].angryLeaving) continue;
        if (selling_game.customers[i].waitingSpawn) continue;

        int d = selling_abs_int(selling_game.player.x - selling_game.customers[i].x);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }

    if (bestDist <= SELLING_SELECT_DISTANCE_TH) return bestIdx;
    return 255;
}

static void selling_refresh_target_preview(void)
{
    uint8_t nearIdx = selling_find_near_customer();

    if (nearIdx != 255) {
        selling_game.player.currentTarget = nearIdx;
        selling_game.player.targetLocked = 1;
    } else {
        selling_game.player.targetLocked = 0;
    }
}

static void selling_init_game(void)
{
    memset(&selling_game, 0, sizeof(selling_game));

    selling_game.gold = 0;
    selling_game.customersServed = 0;
    selling_game.remainingTime = SELLING_GAME_TIME_SECONDS;
    selling_game.running = 1;
    selling_game.finished = 0;
    selling_game.player.x = 120;
    selling_game.player.selectedDish = SELLING_DISH_COD_BURGER;
    selling_game.player.currentTarget = 1;
    selling_game.player.targetLocked = 0;
    strcpy(selling_game.feedbackText, "Ready");

    selling_schedule_initial_spawns(selling_now_ms());

    selling_lastUpdateMs = selling_now_ms();
    selling_lastRenderMs = selling_lastUpdateMs;
    selling_lastSecondMs = selling_lastUpdateMs;
    selling_lastPatienceMs = selling_lastUpdateMs;
    selling_lastTradeMs = 0;
    selling_lastDishChangeMs = 0;
    selling_finishMs = 0;

    selling_penaltyActive = 0;
    selling_penaltyTargetIdx = 0;
    selling_penaltyStartMs = 0;
    selling_penaltyEndMs = 0;

    selling_wrongAlertActive = 0;
    selling_wrongAlertEndMs = 0;

    selling_ledAlertEnabled = 0;
    selling_ledState = 0;
    selling_ledBlinkDivider = 0;
    selling_ledBlinkTargetDiv = SELLING_ALERT_FLASH_DIV;

    selling_angryLastToggleMs = 0;

    selling_gameOverMelodyPlayed = 0;
    selling_resultMelodyPlayed = 0;
    selling_resultSelection = 0;
    selling_lastMenuNavMs = 0;
    selling_resultEnterMs = 0;

    selling_led_alert_stop();
    selling_buzzer_stop();
}

static void selling_update_player(int8_t joyDir)
{
    if (joyDir < 0) selling_game.player.x -= SELLING_PLAYER_MOVE_STEP;
    else if (joyDir > 0) selling_game.player.x += SELLING_PLAYER_MOVE_STEP;

    if (selling_game.player.x < SELLING_PLAYER_MIN_X) selling_game.player.x = SELLING_PLAYER_MIN_X;
    if (selling_game.player.x > SELLING_PLAYER_MAX_X) selling_game.player.x = SELLING_PLAYER_MAX_X;
}

static void selling_cycle_dish(uint32_t nowMs)
{
    if (nowMs - selling_lastDishChangeMs < SELLING_DISH_DEBOUNCE_MS) return;

    selling_lastDishChangeMs = nowMs;
    selling_game.player.selectedDish =
        (SellingDishType)((selling_game.player.selectedDish + 1) % SELLING_DISH_COUNT);

    snprintf(selling_game.feedbackText, sizeof(selling_game.feedbackText),
             "Dish: %s", selling_dish_name(selling_game.player.selectedDish));
}

static void selling_trigger_reward(uint8_t value, uint32_t nowMs)
{
    selling_game.rewardVisible = 1;
    selling_game.rewardValue = value;
    selling_game.rewardStartMs = nowMs;
    selling_game.rewardY = 198;
}

static void selling_start_angry_leave(uint8_t idx, uint32_t nowMs)
{
    SellingCustomer *customer = &selling_game.customers[idx];

    customer->angryLeaving = 1;
    customer->angryStartMs = nowMs;
    customer->active = 1;
    customer->waitingSpawn = 0;

    if (selling_game.player.currentTarget == idx) {
        selling_game.player.targetLocked = 0;
    }
}

static void selling_start_wrong_penalty(uint8_t idx, uint32_t nowMs)
{
    selling_penaltyActive = 1;
    selling_penaltyTargetIdx = idx;
    selling_penaltyStartMs = nowMs;
    selling_penaltyEndMs = nowMs + SELLING_WRONG_FLASH_TOTAL_MS;
    selling_start_wrong_alert(nowMs);
}

static void selling_try_trade(uint32_t nowMs)
{
    SellingCustomer *target;

    if (nowMs - selling_lastTradeMs < SELLING_TRADE_DEBOUNCE_MS) return;
    selling_lastTradeMs = nowMs;

    if (!selling_game.player.targetLocked) {
        strcpy(selling_game.feedbackText, "Move closer");
        return;
    }

    target = &selling_game.customers[selling_game.player.currentTarget];
    if (!target->active || target->angryLeaving || target->waitingSpawn) return;

    if (selling_abs_int(selling_game.player.x - target->x) > SELLING_SELECT_DISTANCE_TH + 10) {
        selling_game.player.targetLocked = 0;
        strcpy(selling_game.feedbackText, "Too far");
        return;
    }

    if (target->count[selling_game.player.selectedDish] > 0) {
        uint8_t reward = selling_dish_price(selling_game.player.selectedDish);

        target->count[selling_game.player.selectedDish]--;
        if (target->totalRemaining > 0) target->totalRemaining--;

        selling_game.gold += reward;
        snprintf(selling_game.feedbackText, sizeof(selling_game.feedbackText), "+%d coins", reward);
        selling_trigger_reward(reward, nowMs);

        if (target->totalRemaining == 0) {
            uint8_t idx = selling_game.player.currentTarget;
            selling_game.customersServed++;
            selling_schedule_spawn(idx, nowMs, SELLING_FINISH_RESPAWN_MIN_MS, SELLING_FINISH_RESPAWN_MAX_MS);
            selling_game.player.targetLocked = 0;
            strcpy(selling_game.feedbackText, "Order done");
        }
    } else {
        if (target->patience > 5) target->patience -= 5;
        else target->patience = 0;

        if (selling_game.remainingTime > SELLING_WRONG_TIME_PENALTY_S) selling_game.remainingTime -= SELLING_WRONG_TIME_PENALTY_S;
        else selling_game.remainingTime = 0;

        selling_start_wrong_penalty(selling_game.player.currentTarget, nowMs);
        strcpy(selling_game.feedbackText, "Wrong dish -2s");

        if (target->patience == 0) {
            uint8_t idx = selling_game.player.currentTarget;
            selling_start_angry_leave(idx, nowMs);
            strcpy(selling_game.feedbackText, "Customer angry");
        }
    }
}

static void selling_update_reward_anim(uint32_t nowMs)
{
    if (!selling_game.rewardVisible) return;

    if (nowMs - selling_game.rewardStartMs < 700) {
        selling_game.rewardY = 198 - (int16_t)((nowMs - selling_game.rewardStartMs) / 35);
    } else {
        selling_game.rewardVisible = 0;
    }
}

static void selling_update_angry_customers(uint32_t nowMs)
{
    for (uint8_t i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
        SellingCustomer *customer = &selling_game.customers[i];
        if (customer->angryLeaving) {
            if ((nowMs - customer->angryStartMs) >= SELLING_ANGRY_LEAVE_MS) {
                selling_schedule_spawn(i, nowMs, SELLING_RESPAWN_MIN_MS, SELLING_RESPAWN_MAX_MS);
            }
        }
    }
}

static void selling_update_game(uint32_t nowMs)
{
    uint16_t vrx = selling_read_vrx_avg();
    int8_t joyDir = selling_get_joy_x_dir(vrx);
    uint8_t joySwPressed = selling_joy_sw_pressed_event(nowMs);
    uint8_t tradePressed = selling_trade_button_pressed_event(nowMs);

    if (!selling_game.running) return;

    selling_update_spawn_queue(nowMs);

    if (nowMs - selling_lastUpdateMs >= SELLING_UPDATE_INTERVAL_MS) {
        selling_lastUpdateMs = nowMs;
        selling_update_player(joyDir);
        selling_refresh_target_preview();
    }

    if (joySwPressed) selling_cycle_dish(nowMs);
    if (tradePressed) selling_try_trade(nowMs);

    if (nowMs - selling_lastPatienceMs >= SELLING_PATIENCE_INTERVAL_MS) {
        selling_lastPatienceMs += SELLING_PATIENCE_INTERVAL_MS;

        for (uint8_t i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
            SellingCustomer *customer = &selling_game.customers[i];

            if (customer->active && !customer->angryLeaving &&
                !customer->waitingSpawn && customer->patience > 0) {
                customer->patience--;
            }

            if (customer->active && !customer->angryLeaving &&
                !customer->waitingSpawn && customer->patience == 0) {
                selling_start_angry_leave(i, nowMs);
                strcpy(selling_game.feedbackText, "Customer angry");
            }
        }
    }

    if (selling_penaltyActive && nowMs >= selling_penaltyEndMs) {
        selling_penaltyActive = 0;
    }

    selling_update_angry_customers(nowMs);
    selling_update_alert_outputs(nowMs);

    if (nowMs - selling_lastSecondMs >= SELLING_SECOND_INTERVAL_MS) {
        selling_lastSecondMs += SELLING_SECOND_INTERVAL_MS;

        if (selling_game.remainingTime > 0) selling_game.remainingTime--;

        if (selling_game.remainingTime <= 0) {
            selling_game.running = 0;
            selling_game.finished = 1;
            selling_finishMs = nowMs;
            selling_resultEnterMs = nowMs;
            strcpy(selling_game.feedbackText, "Selling over");
            selling_led_alert_stop();
            selling_buzzer_stop();

            if (!selling_gameOverMelodyPlayed) {
                selling_gameOverMelodyPlayed = 1;
                selling_play_gameover_melody();
            }
        }
    }

    /* keep timer-driven LED responsive every loop */
    selling_led_update_from_timer();
}

/* =========================================================
 * Result menu input
 * ========================================================= */
static int8_t selling_update_result_menu(uint32_t nowMs)
{
    uint16_t vrx;
    int diff;
    uint8_t joySwPressed = selling_joy_sw_pressed_event(nowMs);

    if ((nowMs - selling_resultEnterMs) < SELLING_RESULT_ENTER_DELAY_MS) {
        return -1;
    }

    /*
     * Confirm first.
     * This prevents a tiny joystick drift during button press from changing
     * MAIN MENU back to PLAY AGAIN at the same moment.
     */
    if (joySwPressed) {
        return (int8_t)selling_resultSelection;
    }

    vrx = selling_read_vrx_avg();
    diff = (int)vrx - (int)selling_center_x;

    if ((nowMs - selling_lastMenuNavMs) >= SELLING_RESULT_MENU_NAV_MS) {
        if (diff <= -SELLING_JOY_ENTER_DEADZONE) {
            selling_resultSelection = 0;   // LEFT = MAIN MENU
            selling_lastMenuNavMs = nowMs;
        }
        else if (diff >= SELLING_JOY_ENTER_DEADZONE) {
            selling_resultSelection = 1;   // RIGHT = PLAY AGAIN
            selling_lastMenuNavMs = nowMs;
        }
    }

    return -1;
}

/* =========================================================
 * UI helpers
 * ========================================================= */
static void selling_clear_screen(uint16_t color)
{
    LCD_Fill_Buffer(color);
}

static void selling_draw_awning(void)
{
    for (int i = 0; i < SELLING_W; i += 24) {
        uint16_t c = ((i / 24) & 1) ? C_AWNING_CREAM : C_AWNING_RED;
        selling_fill_rect(i, 0, 24, 20, c);
        selling_fill_rect(i + 3, 20, 18, 5, c);
    }
    selling_fill_rect(0, 25, SELLING_W, 3, C_AWNING_RED_DARK);
}

static void selling_draw_wall(void)
{
    selling_fill_rect(0, 0, SELLING_W, 122, C_WALL_CREAM);

    for (int y = 30; y < 122; y += 16) {
        selling_hline(0, y, SELLING_W, C_WALL_LINE);
    }

    for (int x = 0; x < SELLING_W; x += 20) {
        selling_vline(x, 30, 92, C_WALL_CREAM_2);
    }

    selling_fill_rect(0, 122, SELLING_W, 14, C_WALL_SHADOW);
    selling_hline(0, 135, SELLING_W, C_WOOD_LINE);
}

static void selling_draw_signboard(void)
{
    selling_fill_rect(66, 24, 2, 8, C_WOOD_DARK);
    selling_fill_rect(172, 24, 2, 8, C_WOOD_DARK);

    selling_draw_round_panel(58, 30, 124, 28, C_SHOP_SIGN_MAIN, C_SHOP_SIGN_DARK);

    selling_fill_rect(64, 35, 112, 5, C_SHOP_SIGN_LIGHT);
    selling_fill_rect(64, 42, 112, 11, C_SHOP_SIGN_MAIN);

    LCD_printString("STAR FISH SHOP", 77, 40, 1, 1);
}

static void selling_draw_menu_board(int x, int y)
{
    selling_draw_round_panel(x, y, 46, 28, C_MENU_BOARD, C_MENU_FRAME);
    selling_fill_rect(x + 4, y + 4, 38, 3, C_MENU_BOARD_2);
    LCD_printString("MENU", x + 10, y + 10, 1, 1);
    LCD_printString("FISH", x + 10, y + 18, 1, 1);
}

static void selling_draw_hanging_plant(int x, int y)
{
    selling_fill_rect(x + 4, y, 2, 6, C_WOOD_LINE);
    selling_fill_rect(x, y + 6, 10, 6, C_WOOD_MID);
    selling_draw_rect(x, y + 6, 10, 6, C_WOOD_DARK);
    selling_fill_rect(x + 1, y + 12, 8, 5, C_ACCENT_GREEN);
}

static void selling_draw_shelves(void)
{
    selling_fill_rect(14, 60, 60, 6, C_WOOD_LIGHT);
    selling_fill_rect(14, 66, 60, 2, C_WOOD_DARK);

    selling_fill_rect(166, 60, 60, 6, C_WOOD_LIGHT);
    selling_fill_rect(166, 66, 60, 2, C_WOOD_DARK);

    selling_fill_rect(22, 46, 10, 14, C_ACCENT_BLUE);
    selling_draw_rect(22, 46, 10, 14, C_WOOD_DARK);

    selling_fill_rect(38, 48, 12, 12, C_ACCENT_GOLD);
    selling_draw_rect(38, 48, 12, 12, C_WOOD_DARK);

    selling_fill_rect(56, 45, 12, 15, C_ACCENT_GREEN);
    selling_draw_rect(56, 45, 12, 15, C_WOOD_DARK);

    selling_fill_rect(172, 46, 10, 14, C_ACCENT_RED);
    selling_draw_rect(172, 46, 10, 14, C_WOOD_DARK);

    selling_fill_rect(188, 45, 12, 15, C_ACCENT_SKY);
    selling_draw_rect(188, 45, 12, 15, C_WOOD_DARK);

    selling_fill_rect(206, 48, 12, 12, C_ACCENT_GOLD);
    selling_draw_rect(206, 48, 12, 12, C_WOOD_DARK);
}

static void selling_draw_floor(void)
{
    for (int y = 152; y < SELLING_H; y += 16) {
        for (int x = 0; x < SELLING_W; x += 16) {
            uint16_t c = (((x / 16) + (y / 16)) & 1) ? C_FLOOR_A : C_FLOOR_B;
            selling_fill_rect(x, y, 16, 16, c);
            selling_draw_rect(x, y, 16, 16, C_FLOOR_LINE);
        }
    }
}

static void selling_draw_shop_background(void)
{
    selling_fill_rect(0, 0, SELLING_W, SELLING_H, C_BLACK);
    selling_draw_wall();
    selling_draw_awning();
    selling_draw_signboard();
    selling_draw_menu_board(12, 36);
    selling_draw_menu_board(182, 36);
    selling_draw_hanging_plant(98, 40);
    selling_draw_hanging_plant(132, 40);
    selling_draw_shelves();
    selling_draw_floor();
}

static void selling_draw_coin_icon(int x, int y)
{
    selling_fill_rect(x - 6, y - 6, 12, 12, C_ACCENT_GOLD);
    selling_draw_rect(x - 6, y - 6, 12, 12, C_HUD_ICON);
    selling_fill_rect(x - 1, y - 4, 2, 8, C_HUD_ICON);
    selling_hline(x - 4, y, 8, C_HUD_ICON);
}

static void selling_draw_clock_icon(int x, int y)
{
    selling_fill_rect(x - 6, y - 6, 12, 12, C_PANEL_INNER);
    selling_draw_rect(x - 6, y - 6, 12, 12, C_HUD_ICON);
    selling_vline(x, y - 3, 4, C_HUD_ICON);
    selling_hline(x, y, 3, C_HUD_ICON);
}

static void selling_draw_hud_box(int x, int y, int w, int h,
                                 const char *title, const char *value, uint8_t isGold)
{
    selling_draw_round_panel(x, y, w, h, C_HUD_FILL, C_HUD_EDGE);
    selling_fill_rect(x + 4, y + 4, w - 8, 7, C_HUD_TOP);

    if (isGold) selling_draw_coin_icon(x + 14, y + 16);
    else        selling_draw_clock_icon(x + 14, y + 16);

    LCD_printString((char *)title, x + 28, y + 8, 1, 1);
    selling_draw_right_aligned_text(value, x + w - 18, y + 7, 2);
}

static void selling_draw_plate_shadow(int cx, int cy)
{
    selling_fill_rect(cx - 13, cy + 8, 26, 4, C_GREY_LIGHT);
}

static void selling_draw_plate(int cx, int cy)
{
    selling_draw_plate_shadow(cx, cy);
    selling_fill_rect(cx - 15, cy - 9, 30, 18, C_PLATE);
    selling_draw_rect(cx - 15, cy - 9, 30, 18, C_GREY);
    selling_fill_rect(cx - 11, cy - 5, 22, 10, C_PLATE_INNER);
}

static void selling_draw_fish_chips_icon(int cx, int cy)
{
    selling_draw_plate(cx, cy);

    selling_fill_rect(cx - 13, cy - 4, 11, 12, C_CHIPS_BOX);
    selling_draw_rect(cx - 13, cy - 4, 11, 12, C_CHIPS_BOX_DARK);

    selling_fill_rect(cx - 11, cy - 11, 2, 7, C_CHIPS_FRIES);
    selling_fill_rect(cx - 8,  cy - 13, 2, 9, C_CHIPS_FRIES);
    selling_fill_rect(cx - 5,  cy - 11, 2, 7, C_CHIPS_FRIES);

    selling_fill_rect(cx + 0, cy - 2, 10, 7, C_FISH_BODY);
    selling_draw_rect(cx + 0, cy - 2, 10, 7, C_FISH_BODY_DARK);

    selling_put_pixel(cx + 2, cy, C_FISH_EYE);
    selling_hline(cx + 2, cy + 3, 6, C_FISH_BODY_DARK);

    selling_fill_rect(cx + 10, cy - 1, 3, 2, C_FISH_FIN);
    selling_fill_rect(cx + 10, cy + 2, 3, 2, C_FISH_FIN);
}

static void selling_draw_burger_icon(int cx, int cy)
{
    selling_draw_plate(cx, cy);

    selling_fill_rect(cx - 10, cy - 9, 20, 4, C_BURGER_BUN_TOP);
    selling_draw_rect(cx - 10, cy - 9, 20, 4, C_WOOD_DARK);

    selling_put_pixel(cx - 6, cy - 8, C_BURGER_SESAME);
    selling_put_pixel(cx - 2, cy - 7, C_BURGER_SESAME);
    selling_put_pixel(cx + 2, cy - 8, C_BURGER_SESAME);
    selling_put_pixel(cx + 6, cy - 7, C_BURGER_SESAME);

    selling_fill_rect(cx - 9, cy - 4, 18, 2, C_BURGER_CHEESE);
    selling_fill_rect(cx - 10, cy - 1, 20, 2, C_BURGER_LETTUCE);
    selling_fill_rect(cx - 9, cy + 2, 18, 3, C_BURGER_PATTY);
    selling_fill_rect(cx - 8, cy + 5, 16, 1, C_BURGER_TOMATO);

    selling_fill_rect(cx - 10, cy + 7, 20, 3, C_BURGER_BUN_BOT);
    selling_draw_rect(cx - 10, cy + 7, 20, 3, C_WOOD_DARK);
}

static void selling_draw_soup_icon(int cx, int cy)
{
    selling_draw_plate(cx, cy);

    selling_fill_rect(cx - 10, cy - 1, 20, 8, C_SOUP_BOWL);
    selling_draw_rect(cx - 10, cy - 1, 20, 8, C_SOUP_BOWL_DARK);

    selling_hline(cx - 8, cy, 16, C_WHITE);
    selling_fill_rect(cx - 8, cy + 1, 16, 3, C_SOUP_SURFACE);
    selling_hline(cx - 7, cy + 2, 14, C_SOUP_SURFACE_2);

    selling_put_pixel(cx - 4, cy + 2, C_SOUP_GARNISH);
    selling_put_pixel(cx - 1, cy + 3, C_SOUP_GARNISH);
    selling_put_pixel(cx + 3, cy + 2, C_SOUP_GARNISH);

    selling_put_pixel(cx - 5, cy - 8, C_SOUP_STEAM);
    selling_put_pixel(cx - 4, cy - 9, C_SOUP_STEAM);
    selling_put_pixel(cx,     cy - 10, C_SOUP_STEAM);
    selling_put_pixel(cx + 1, cy - 9, C_SOUP_STEAM);
    selling_put_pixel(cx + 5, cy - 8, C_SOUP_STEAM);
    selling_put_pixel(cx + 4, cy - 9, C_SOUP_STEAM);
}

static void selling_draw_dish_icon(SellingDishType dish, int cx, int cy)
{
    /* Game2-style dish icon, but keep original Game3 icon size and position. */
    selling_draw_plate_shadow(cx, cy);
    selling_fill_rect(cx - 15, cy - 9, 30, 18, C_PLATE);
    selling_draw_rect(cx - 15, cy - 9, 30, 18, C_GREY);
    selling_fill_rect(cx - 11, cy - 5, 22, 10, C_PLATE_INNER);

    if (dish == SELLING_DISH_FISH_CHIPS) {
        selling_fill_rect(cx - 12, cy - 4, 11, 12, C_CHIPS_BOX);
        selling_draw_rect(cx - 12, cy - 4, 11, 12, C_CHIPS_BOX_DARK);
        selling_fill_rect(cx - 10, cy - 12, 2, 8, C_CHIPS_FRIES);
        selling_fill_rect(cx - 7,  cy - 14, 2, 10, C_CHIPS_FRIES);
        selling_fill_rect(cx - 4,  cy - 11, 2, 7, C_CHIPS_FRIES);
        selling_fill_rect(cx - 9,  cy - 2, 7, 2, C_CHIPS_BOX_DARK);

        selling_fill_rect(cx + 0, cy - 2, 10, 7, C_FISH_BODY);
        selling_draw_rect(cx + 0, cy - 2, 10, 7, C_FISH_BODY_DARK);
        selling_fill_rect(cx + 10, cy - 1, 4, 2, C_FISH_FIN);
        selling_fill_rect(cx + 10, cy + 2, 4, 2, C_FISH_FIN);
        selling_put_pixel(cx + 2, cy, C_FISH_EYE);
        selling_hline(cx + 3, cy + 3, 5, C_FISH_BODY_DARK);
    }
    else if (dish == SELLING_DISH_COD_BURGER) {
        selling_fill_rect(cx - 11, cy - 9, 22, 4, C_BURGER_BUN_TOP);
        selling_draw_rect(cx - 11, cy - 9, 22, 4, C_WOOD_DARK);
        selling_put_pixel(cx - 7, cy - 8, C_BURGER_SESAME);
        selling_put_pixel(cx - 2, cy - 8, C_BURGER_SESAME);
        selling_put_pixel(cx + 3, cy - 8, C_BURGER_SESAME);
        selling_put_pixel(cx + 8, cy - 7, C_BURGER_SESAME);

        selling_fill_rect(cx - 12, cy - 4, 24, 2, C_BURGER_LETTUCE);
        selling_fill_rect(cx - 10, cy - 1, 20, 2, C_BURGER_CHEESE);
        selling_fill_rect(cx - 11, cy + 2, 22, 4, C_BURGER_PATTY);
        selling_fill_rect(cx - 9,  cy + 6, 18, 2, C_BURGER_TOMATO);
        selling_fill_rect(cx - 11, cy + 8, 22, 3, C_BURGER_BUN_BOT);
        selling_draw_rect(cx - 11, cy + 8, 22, 3, C_WOOD_DARK);
    }
    else {
        selling_fill_rect(cx - 12, cy - 1, 24, 9, C_SOUP_BOWL);
        selling_draw_rect(cx - 12, cy - 1, 24, 9, C_SOUP_BOWL_DARK);
        selling_hline(cx - 9, cy, 18, C_WHITE);
        selling_fill_rect(cx - 9, cy + 1, 18, 3, C_SOUP_SURFACE);
        selling_hline(cx - 7, cy + 2, 14, C_SOUP_SURFACE_2);
        selling_put_pixel(cx - 5, cy + 2, C_SOUP_GARNISH);
        selling_put_pixel(cx,     cy + 3, C_SOUP_GARNISH);
        selling_put_pixel(cx + 5, cy + 2, C_SOUP_GARNISH);

        selling_put_pixel(cx - 6, cy - 8, C_SOUP_STEAM);
        selling_put_pixel(cx - 5, cy - 9, C_SOUP_STEAM);
        selling_put_pixel(cx,     cy - 10, C_SOUP_STEAM);
        selling_put_pixel(cx + 5, cy - 9, C_SOUP_STEAM);
        selling_put_pixel(cx + 6, cy - 8, C_SOUP_STEAM);
    }
}

static void selling_draw_customer_head_base(int cx, int cy, uint16_t hairColor)
{
    selling_fill_circle(cx, cy - 1, 10, C_SKIN);
    selling_draw_rect(cx - 10, cy - 11, 20, 20, C_SKIN_DARK);

    selling_fill_rect(cx - 12, cy - 14, 24, 6, hairColor);
    selling_fill_rect(cx - 9, cy - 8, 18, 2, hairColor);

    selling_fill_rect(cx - 5, cy - 2, 2, 2, C_BLACK);
    selling_fill_rect(cx + 3, cy - 2, 2, 2, C_BLACK);
    selling_fill_rect(cx - 1, cy + 2, 2, 2, C_SKIN_DARK);
}

static void selling_draw_customer_body(int cx, int cy, uint16_t cloth, uint16_t scarf)
{
    selling_fill_rect(cx - 12, cy + 9, 24, 18, cloth);
    selling_draw_rect(cx - 12, cy + 9, 24, 18, C_WOOD_DARK);
    selling_fill_rect(cx - 8, cy + 10, 16, 4, scarf);
}

static void selling_draw_customer_style0(int cx, int cy)
{
    selling_draw_customer_head_base(cx, cy, C_HAIR_BROWN);
    selling_draw_customer_body(cx, cy, C_CUST_CLOTH_1, C_CUST_CLOTH_1B);
}

static void selling_draw_customer_style1(int cx, int cy)
{
    selling_draw_customer_head_base(cx, cy, C_HAIR);
    selling_draw_customer_body(cx, cy, C_CUST_CLOTH_2, C_CUST_CLOTH_2B);
    selling_fill_rect(cx - 5, cy + 13, 10, 5, C_PANEL_BG);
}

static void selling_draw_customer_style2(int cx, int cy)
{
    selling_draw_customer_head_base(cx, cy, C_HAIR_BLONDE);
    selling_draw_customer_body(cx, cy, C_CUST_CLOTH_3, C_CUST_CLOTH_3B);
    selling_draw_rect(cx - 7, cy - 3, 5, 5, C_GREY);
    selling_draw_rect(cx + 2, cy - 3, 5, 5, C_GREY);
    selling_hline(cx - 1, cy - 1, 2, C_GREY);
}

static void selling_draw_customer_sprite(const SellingCustomer *c, int style, int selected)
{
    int cx = c->x;
    int cy = c->y;
    uint8_t penaltySelected = 0;

    if (!c->active || c->waitingSpawn) return;

    if (selling_penaltyActive && c == &selling_game.customers[selling_penaltyTargetIdx]) {
        penaltySelected = 1;
    }

    if (selected) {
        selling_draw_rect_thick(cx - 22, cy - 24, 44, 56, 2, C_HIGHLIGHT);
        selling_draw_star_spark(cx - 24, cy - 18, C_HIGHLIGHT);
        selling_draw_star_spark(cx + 24, cy - 10, C_HIGHLIGHT);
    }

    if (penaltySelected) {
        uint32_t phase = ((selling_now_ms() - selling_penaltyStartMs) / SELLING_WRONG_FLASH_STEP_MS) & 1U;
        int shake = (phase == 0U) ? -2 : 2;
        cx += shake;

        if (phase == 0U) selling_draw_rect_thick(cx - 24, cy - 26, 48, 60, 3, C_DANGER);
        else selling_draw_rect_thick(cx - 22, cy - 24, 44, 56, 2, C_WHITE);
    }

    if (style == 0) selling_draw_customer_style0(cx, cy);
    else if (style == 1) selling_draw_customer_style1(cx, cy);
    else selling_draw_customer_style2(cx, cy);

    if (c->angryLeaving) {
        selling_hline(cx - 7, cy - 2, 4, C_BLACK);
        selling_hline(cx + 3, cy - 2, 4, C_BLACK);
        selling_hline(cx - 4, cy + 6, 8, C_ACCENT_RED);
        LCD_printString("!!", cx - 4, cy - 28, 1, 1);
    }
}

static void selling_build_order_lines(const SellingCustomer *c, char out[3][16], int *count)
{
    int n = 0;
    for (int d = 0; d < SELLING_DISH_COUNT; d++) {
        if (c->count[d] > 0 && n < 3) {
            snprintf(out[n], 16, "%s x%d", selling_dish_short((SellingDishType)d), c->count[d]);
            n++;
        }
    }
    *count = n;
}

static void selling_draw_order_card(const SellingCustomer *c, int x, int y)
{
    char lines[3][16];
    int lineCount = 0;
    uint8_t penaltySelected = 0;

    if (selling_penaltyActive && c == &selling_game.customers[selling_penaltyTargetIdx]) {
        penaltySelected = 1;
    }

    selling_draw_pixel_frame(x, y, CARD_W, CARD_H, C_PANEL_BG, C_PANEL_EDGE, C_PANEL_INNER);
    selling_fill_rect(x + 4, y + 4, CARD_W - 8, 8, C_PANEL_BAR);
    selling_draw_centered_text_1("ORDER", x + 4, y + 5, CARD_W - 8);

    if (c->angryLeaving) {
        selling_draw_centered_text_1("ANGRY!!", x, y + 19, CARD_W);
    } else if (!c->active && c->waitingSpawn) {
        selling_draw_centered_text_1("COMING", x, y + 19, CARD_W);
    } else if (!c->active) {
        selling_draw_centered_text_1("EMPTY", x, y + 19, CARD_W);
    } else {
        selling_build_order_lines(c, lines, &lineCount);

        if (lineCount == 1) {
            selling_draw_centered_text_1(lines[0], x, y + 19, CARD_W);
        } else if (lineCount == 2) {
            selling_draw_centered_text_1(lines[0], x, y + 15, CARD_W);
            selling_draw_centered_text_1(lines[1], x, y + 25, CARD_W);
        } else {
            selling_draw_centered_text_1(lines[0], x, y + 13, CARD_W);
            selling_draw_centered_text_1(lines[1], x, y + 22, CARD_W);
            selling_draw_centered_text_1(lines[2], x, y + 31, CARD_W);
        }
    }

    if (penaltySelected) {
        uint32_t phase = ((selling_now_ms() - selling_penaltyStartMs) / SELLING_WRONG_FLASH_STEP_MS) & 1U;
        if (phase == 0U) selling_draw_rect_thick(x - 2, y - 2, CARD_W + 4, CARD_H + 4, 3, C_DANGER);
        else selling_draw_rect_thick(x - 1, y - 1, CARD_W + 2, CARD_H + 2, 2, C_WHITE);
    }
}

static void selling_draw_patience_bar(const SellingCustomer *c, int x, int y)
{
    int fill = (c->patience * (PATIENCE_W - 4)) / SELLING_MAX_PATIENCE;
    uint16_t color = C_PATIENCE_BLUE;

    selling_fill_rect(x, y, PATIENCE_W, PATIENCE_H, C_WOOD_DARK);
    selling_draw_rect(x, y, PATIENCE_W, PATIENCE_H, C_BLACK);
    selling_fill_rect(x + 2, y + 2, PATIENCE_W - 4, PATIENCE_H - 4, C_PATIENCE_BG);

    if (!c->active || c->waitingSpawn) return;

    if (c->angryLeaving) {
        selling_fill_rect(x + 2, y + 2, PATIENCE_W - 4, PATIENCE_H - 4, C_PATIENCE_RED);
        return;
    }

    if (selling_game.remainingTime <= 10) color = C_PATIENCE_RED;

    if (fill < 0) fill = 0;
    if (fill > (PATIENCE_W - 4)) fill = PATIENCE_W - 4;

    selling_fill_rect(x + 2, y + 2, fill, PATIENCE_H - 4, color);
}

static void selling_draw_counter_posts(void)
{
    selling_fill_rect(14, COUNTER_Y - 8, 10, 66, C_WOOD_MID);
    selling_draw_rect(14, COUNTER_Y - 8, 10, 66, C_WOOD_DARK);
    selling_fill_rect(16, COUNTER_Y - 6, 2, 62, C_POST_SHADE);

    selling_fill_rect(216, COUNTER_Y - 8, 10, 66, C_WOOD_MID);
    selling_draw_rect(216, COUNTER_Y - 8, 10, 66, C_WOOD_DARK);
    selling_fill_rect(218, COUNTER_Y - 6, 2, 62, C_POST_SHADE);
}

static void selling_draw_counter(void)
{
    selling_draw_counter_posts();

    selling_fill_rect(0, COUNTER_Y, SELLING_W, 58, C_WOOD_MID);
    selling_fill_rect(0, COUNTER_Y, SELLING_W, 10, C_WOOD_TOP);
    selling_draw_rect(0, COUNTER_Y, SELLING_W, 58, C_WOOD_DARK);

    for (int y = COUNTER_Y + 14; y < 194; y += 12) {
        selling_hline(0, y, SELLING_W, C_WOOD_LIGHT);
    }

    for (int x = 0; x < SELLING_W; x += 24) {
        selling_vline(x, COUNTER_Y + 18, 40, C_WOOD_LINE);
    }

    selling_draw_round_panel(58, 138, 124, 18, C_DESK_SIGN_MAIN, C_DESK_SIGN_DARK);
    selling_fill_rect(64, 142, 112, 4, C_DESK_SIGN_LIGHT);
    LCD_printString("SERVICE DESK", 83, 143, 1, 1);
}

static void selling_draw_dish_station(int cx, int cy, SellingDishType dish, const char *label, uint16_t tabColor)
{
    (void)tabColor;

    selling_draw_round_panel(cx - 24, cy - 20, 48, 34, C_PANEL_BG, C_PANEL_EDGE);
    selling_fill_rect(cx - 20, cy - 16, 40, 26, C_PANEL_INNER);
    selling_draw_dish_icon(dish, cx, cy - 2);

    selling_draw_round_panel(cx - 21, cy + 20, 42, 12, C_DISH_TAG_FILL, C_WOOD_DARK);
    selling_draw_centered_text_1(label, cx - 21, cy + 22, 42);
}

static void selling_draw_loading_like_screen(const char *centerTitle, const char *centerValue,
                                             const char *line1, const char *line2,
                                             uint8_t showBar, uint8_t percent)
{
    char buf[16];
    int barX = 28;
    int barY = 180;
    int barW = 184;
    int barH = 16;
    int fillW = (percent * (barW - 4)) / 100;

    selling_clear_screen(C_LOAD_BG);
    selling_fill_rect(0, 0, SELLING_W, 240, C_LOAD_BG);
    selling_draw_awning();
    selling_draw_signboard();

    selling_draw_round_panel(34, 118, 172, 26, C_PANEL_BG, C_PANEL_EDGE);
    selling_draw_centered_text_2(centerTitle, 34, 125, 172);

    selling_draw_dish_station(48, 82, SELLING_DISH_FISH_CHIPS, "Chips", C_SIGN_GOLD);
    selling_draw_dish_station(120, 82, SELLING_DISH_COD_BURGER, "Burger", C_ACCENT_GREEN);
    selling_draw_dish_station(192, 82, SELLING_DISH_COD_SOUP, "Soup", C_ACCENT_RED);

    if (showBar) {
        selling_draw_round_panel(barX, barY, barW, barH, C_LOAD_BAR_BG, C_PANEL_EDGE);
        selling_fill_rect(barX + 2, barY + 2, fillW, barH - 4, C_LOAD_BAR_FG);

        snprintf(buf, sizeof(buf), "%d%%", percent);
        selling_draw_centered_text_1(buf, 0, 200, SELLING_W);
    } else {
        selling_draw_round_panel(86, 148, 68, 38, C_PANEL_BG, C_PANEL_EDGE);
        LCD_printString((char *)centerValue, 95, 158, 1, 3);

        selling_draw_centered_text_1(line1, 0, 198, SELLING_W);
        selling_draw_centered_text_1(line2, 0, 214, SELLING_W);
    }

    LCD_Refresh(&cfg0);
}

static void selling_draw_loading_screen(uint8_t percent)
{
    selling_draw_loading_like_screen("Let's Selling!", "", "", "", 1, percent);
}

static void selling_loading_sequence(void)
{
    uint32_t steps = SELLING_LOADING_TOTAL_MS / SELLING_LOADING_STEP_MS;
    uint8_t stagePlayed[5] = {0, 0, 0, 0, 0};

    if (steps == 0) steps = 1;

    for (uint32_t i = 0; i <= steps; i++) {
        uint8_t percent = (uint8_t)((i * 100U) / steps);
        selling_led_loading_breath(percent);
        selling_draw_loading_screen(percent);

        if (percent >= 10 && !stagePlayed[0]) { stagePlayed[0] = 1; selling_play_loading_tick_sfx(0); }
        if (percent >= 30 && !stagePlayed[1]) { stagePlayed[1] = 1; selling_play_loading_tick_sfx(1); }
        if (percent >= 55 && !stagePlayed[2]) { stagePlayed[2] = 1; selling_play_loading_tick_sfx(2); }
        if (percent >= 80 && !stagePlayed[3]) { stagePlayed[3] = 1; selling_play_loading_tick_sfx(3); }
        if (percent >= 100 && !stagePlayed[4]) { stagePlayed[4] = 1; selling_play_loading_finish_melody(); }

        HAL_Delay(SELLING_LOADING_STEP_MS);
    }

    selling_led_pwm_off();
}

static void selling_draw_countdown_screen(const char *bigText, const char *smallText)
{
    selling_draw_loading_like_screen("Let's Selling!", bigText,
                                     (char *)smallText,
                                     "Serve fast and carefully",
                                     0, 0);
}

static void selling_start_animation(void)
{
    selling_draw_countdown_screen("3", "Customers are waiting");
    selling_play_countdown_beep(3);
    HAL_Delay(SELLING_COUNTDOWN_MS);

    selling_draw_countdown_screen("2", "Pick the right dish");
    selling_play_countdown_beep(2);
    HAL_Delay(SELLING_COUNTDOWN_MS);

    selling_draw_countdown_screen("1", "Move and trade fast");
    selling_play_countdown_beep(1);
    HAL_Delay(SELLING_COUNTDOWN_MS);

    selling_draw_countdown_screen("GO!", "Start selling now");
    selling_play_go_sfx();
    HAL_Delay(SELLING_START_FLASH_MS);
}

static void selling_draw_header(void)
{
    char bufGold[16];
    char bufTime[16];

    snprintf(bufGold, sizeof(bufGold), "%d", selling_game.gold);
    selling_format_time(selling_game.remainingTime, bufTime, sizeof(bufTime));

    selling_draw_hud_box(HUD_L_X, HUD_L_Y, HUD_L_W, HUD_L_H, "GOLD", bufGold, 1);
    selling_draw_hud_box(HUD_R_X, HUD_R_Y, HUD_R_W, HUD_R_H, "TIME", bufTime, 0);
}

static void selling_draw_penalty_banner(void)
{
    if (!selling_penaltyActive) return;

    selling_draw_round_panel(24, 44, 192, 18, C_WARN_BG, C_WARN_EDGE);
    selling_fill_rect(28, 48, 184, 4, C_WARN_FILL);
    LCD_printString("MISS! TIME -2", 72, 49, 1, 1);
}

static void selling_draw_patience_penalty_fx(void)
{
    if (!selling_penaltyActive) return;
    if (selling_penaltyTargetIdx >= SELLING_MAX_CUSTOMERS) return;

    SellingCustomer *c = &selling_game.customers[selling_penaltyTargetIdx];
    if (!c->active || c->waitingSpawn) return;

    uint32_t elapsed = selling_now_ms() - selling_penaltyStartMs;
    int rise = (int)(elapsed / 45);
    if (rise > 10) rise = 10;

    int px = c->x - 24;
    int py = c->y - 42 - rise;
    uint32_t phase = (elapsed / SELLING_WRONG_FLASH_STEP_MS) & 1U;
    uint16_t fill = (phase == 0U) ? C_DANGER : C_WARN_FILL;

    selling_draw_round_panel(px, py, 48, 14, fill, C_WOOD_DARK);
    LCD_printString("PAT -5", px + 7, py + 4, 1, 1);
    selling_draw_star_spark(px - 3, py + 5, C_DANGER);
    selling_draw_star_spark(px + 51, py + 6, C_DANGER);
}

static void selling_draw_feedback(void)
{
    if (selling_game.rewardVisible) {
        char rewardBuf[12];
        snprintf(rewardBuf, sizeof(rewardBuf), "+%d", selling_game.rewardValue);
        LCD_printString(rewardBuf, 108, selling_game.rewardY, 1, 2);
        selling_draw_star_spark(104, selling_game.rewardY + 4, C_ACCENT_GOLD);
        selling_draw_star_spark(136, selling_game.rewardY + 10, C_ACCENT_GOLD);
    } else {
        selling_draw_round_panel(FEEDBACK_X, FEEDBACK_Y, FEEDBACK_W, FEEDBACK_H,
                                 C_FEEDBACK_BG, C_FEEDBACK_BORDER);
        selling_draw_rect(FEEDBACK_X + 2, FEEDBACK_Y + 2,
                          FEEDBACK_W - 4, FEEDBACK_H - 4, C_FEEDBACK_INNER);

        if (strlen(selling_game.feedbackText) <= 10) {
            selling_draw_centered_text_1(selling_game.feedbackText, FEEDBACK_X, FEEDBACK_Y + 5, FEEDBACK_W);
        } else if (strlen(selling_game.feedbackText) <= 18) {
            selling_draw_centered_text_1(selling_game.feedbackText, FEEDBACK_X + 4, FEEDBACK_Y + 5, FEEDBACK_W - 8);
        } else {
            selling_draw_centered_text_1(selling_game.feedbackText, FEEDBACK_X + 2, FEEDBACK_Y + 5, FEEDBACK_W - 4);
        }
    }
}

static void selling_draw_player(int cx, int cy)
{
    selling_fill_rect(cx - 9, cy + 24, 6, 8, C_FISHERMAN_BOOT);
    selling_fill_rect(cx + 3, cy + 24, 6, 8, C_FISHERMAN_BOOT);
    selling_draw_rect(cx - 9, cy + 24, 6, 8, C_WOOD_DARK);
    selling_draw_rect(cx + 3, cy + 24, 6, 8, C_WOOD_DARK);

    selling_fill_rect(cx - 10, cy + 14, 20, 12, C_FISHERMAN_PANTS);
    selling_draw_rect(cx - 10, cy + 14, 20, 12, C_WOOD_DARK);

    selling_fill_rect(cx - 13, cy + 6, 26, 16, C_FISHERMAN_COAT);
    selling_draw_rect(cx - 13, cy + 6, 26, 16, C_WOOD_DARK);

    selling_vline(cx - 4, cy + 8, 14, C_NET_COLOR);
    selling_vline(cx + 4, cy + 8, 14, C_NET_COLOR);

    selling_fill_rect(cx - 16, cy + 10, 3, 6, C_SKIN);
    selling_fill_rect(cx + 13, cy + 10, 3, 6, C_SKIN);

    selling_draw_rect(cx + 15, cy + 12, 7, 7, C_NET_COLOR);
    selling_hline(cx + 15, cy + 15, 7, C_NET_COLOR);
    selling_vline(cx + 18, cy + 12, 7, C_NET_COLOR);

    selling_fill_circle(cx, cy - 2, 9, C_SKIN);
    selling_draw_rect(cx - 9, cy - 11, 18, 18, C_SKIN_DARK);

    selling_fill_rect(cx - 5, cy + 1, 10, 4, C_BEARD);

    selling_fill_rect(cx - 4, cy - 4, 2, 2, C_BLACK);
    selling_fill_rect(cx + 2, cy - 4, 2, 2, C_BLACK);

    selling_fill_rect(cx - 1, cy - 1, 2, 2, C_SKIN_DARK);

    selling_fill_rect(cx - 12, cy - 15, 24, 5, C_HAT);
    selling_fill_rect(cx - 8,  cy - 10, 16, 4, C_HAT_DARK);
    selling_draw_rect(cx - 12, cy - 15, 24, 5, C_WOOD_DARK);
    selling_draw_rect(cx - 8,  cy - 10, 16, 4, C_WOOD_DARK);
}

static void selling_render_game(void)
{
    selling_clear_screen(C_BLACK);
    selling_draw_shop_background();

    selling_draw_header();
    selling_draw_penalty_banner();

    for (int i = 0; i < SELLING_MAX_CUSTOMERS; i++) {
        SellingCustomer *c = &selling_game.customers[i];
        int cardX = (i == 0) ? CARD0_X : (i == 1 ? CARD1_X : CARD2_X);
        int patienceX = (i == 0) ? PATIENCE0_X : (i == 1 ? PATIENCE1_X : PATIENCE2_X);

        selling_draw_order_card(c, cardX, CARD_Y);
        selling_draw_patience_bar(c, patienceX, PATIENCE_Y);
        selling_draw_customer_sprite(c, i,
            (selling_game.player.targetLocked && selling_game.player.currentTarget == i));
    }

    selling_draw_patience_penalty_fx();
    selling_draw_counter();

    selling_draw_dish_station(DISH_L_X, DISH_Y, SELLING_DISH_FISH_CHIPS, "Chips", C_SIGN_GOLD);
    selling_draw_dish_station(DISH_C_X, DISH_Y, SELLING_DISH_COD_BURGER, "Burger", C_ACCENT_GREEN);
    selling_draw_dish_station(DISH_R_X, DISH_Y, SELLING_DISH_COD_SOUP, "Soup", C_ACCENT_RED);

    {
        int x = 0;
        if (selling_game.player.selectedDish == SELLING_DISH_FISH_CHIPS) x = DISH_L_X - 26;
        else if (selling_game.player.selectedDish == SELLING_DISH_COD_BURGER) x = DISH_C_X - 26;
        else x = DISH_R_X - 26;

        selling_draw_rect_thick(x - 1, DISH_FRAME_Y - 1, DISH_FRAME_W + 2, DISH_FRAME_H + 2, 3, C_HIGHLIGHT);
        selling_draw_rect(x + 2, DISH_FRAME_Y + 2, DISH_FRAME_W - 4, DISH_FRAME_H - 4, C_WHITE);

        selling_draw_star_spark(x - 4, DISH_FRAME_Y + 4, C_HIGHLIGHT);
        selling_draw_star_spark(x + DISH_FRAME_W + 4, DISH_FRAME_Y + 10, C_HIGHLIGHT);
        selling_draw_star_spark(x - 3, DISH_FRAME_Y + 48, C_HIGHLIGHT);
        selling_draw_star_spark(x + DISH_FRAME_W + 3, DISH_FRAME_Y + 40, C_HIGHLIGHT);
    }

    selling_draw_feedback();
    selling_draw_player(selling_game.player.x, 196);

    LCD_Refresh(&cfg0);
}

static void selling_render_result(void)
{
    char buf1[32];
    char buf2[32];
    char buf3[32];
    const char *resultMsg = selling_get_result_message(selling_game.customersServed, selling_game.gold);

    selling_clear_screen(C_LOAD_BG);

    selling_draw_wall();
    selling_draw_awning();
    selling_draw_signboard();
    selling_draw_floor();

    selling_fill_rect(0, 168, SELLING_W, 40, C_WOOD_MID);
    selling_fill_rect(0, 168, SELLING_W, 8, C_WOOD_TOP);
    selling_draw_rect(0, 168, SELLING_W, 40, C_WOOD_DARK);

    for (int x = 0; x < SELLING_W; x += 24) {
        selling_vline(x, 176, 32, C_WOOD_LINE);
    }

    selling_draw_round_panel(42, 30, 156, 28, C_SIGN_WOOD, C_WOOD_DARK);
    selling_fill_rect(48, 35, 144, 4, C_SIGN_GOLD);
    selling_draw_centered_text_2("Day Result", 42, 40, 156);

    selling_draw_round_panel(18, 68, 204, 102, C_PANEL_BG, C_PANEL_EDGE);
    selling_fill_rect(26, 78, 188, 10, C_PANEL_BAR);
    selling_draw_centered_text_1("FISH STALL SUMMARY", 26, 80, 188);

    selling_draw_dish_icon(SELLING_DISH_FISH_CHIPS, 38, 52);
    selling_draw_dish_icon(SELLING_DISH_COD_BURGER, 202, 52);

    selling_draw_round_panel(42, 98, 156, 18, C_DISH_TAG_FILL, C_WOOD_DARK);
    selling_draw_round_panel(42, 122, 156, 18, C_DISH_TAG_FILL, C_WOOD_DARK);
    selling_draw_round_panel(42, 146, 156, 18, C_DISH_TAG_FILL, C_WOOD_DARK);

    snprintf(buf1, sizeof(buf1), "Served : %d", selling_game.customersServed);
    snprintf(buf2, sizeof(buf2), "Gold   : %d", selling_game.gold);
    snprintf(buf3, sizeof(buf3), "%s", resultMsg);

    LCD_printString(buf1, 64, 103, 1, 1);
    LCD_printString(buf2, 64, 127, 1, 1);
    selling_draw_centered_text_1(buf3, 42, 151, 156);

    {
        int leftX = 26;
        int rightX = 126;
        int btnY = 188;
        int btnW = 88;
        int btnH = 24;

        if (selling_resultSelection == 0) {
            selling_draw_round_panel(leftX, btnY, btnW, btnH, C_HIGHLIGHT, C_WOOD_DARK);
            selling_draw_rect_thick(leftX - 2, btnY - 2, btnW + 4, btnH + 4, 2, C_HIGHLIGHT);
        } else {
            selling_draw_round_panel(leftX, btnY, btnW, btnH, C_PANEL_INNER, C_WOOD_DARK);
        }

        if (selling_resultSelection == 1) {
            selling_draw_round_panel(rightX, btnY, btnW, btnH, C_HIGHLIGHT, C_WOOD_DARK);
            selling_draw_rect_thick(rightX - 2, btnY - 2, btnW + 4, btnH + 4, 2, C_HIGHLIGHT);
        } else {
            selling_draw_round_panel(rightX, btnY, btnW, btnH, C_PANEL_INNER, C_WOOD_DARK);
        }

        selling_draw_centered_text_1("MAIN MENU", leftX, btnY + 8, btnW);
        selling_draw_centered_text_1("PLAY AGAIN", rightX, btnY + 8, btnW);
    }

    selling_draw_centered_text_1("LEFT/RIGHT TO CHOOSE", 0, 218, SELLING_W);
    selling_draw_centered_text_1("JOY SW TO CONFIRM", 0, 230, SELLING_W);

    selling_draw_star_spark(24, 64, C_ACCENT_GOLD);
    selling_draw_star_spark(216, 64, C_ACCENT_GOLD);
    selling_draw_star_spark(22, 176, C_ACCENT_GOLD);
    selling_draw_star_spark(218, 176, C_ACCENT_GOLD);

    LCD_Refresh(&cfg0);
}

/* =========================================================
 * Public entry
 * ========================================================= */
MenuState Game3_Run(void)
{
    MenuState exit_state = MENU_STATE_HOME;

    selling_led_alert_stop();
    selling_buzzer_stop();

    selling_loading_sequence();
    selling_calibrate_joystick_center();
    selling_start_animation();
    selling_init_game();

    while (1) {
        uint32_t nowMs = selling_now_ms();

        if (selling_game.running) {
            selling_update_game(nowMs);

            if ((nowMs - selling_lastRenderMs) >= SELLING_RENDER_INTERVAL_MS) {
                selling_lastRenderMs = nowMs;
                selling_render_game();
            }
        } else {
            if (!selling_resultMelodyPlayed) {
                selling_resultMelodyPlayed = 1;
                selling_play_result_melody();
            }

            selling_render_result();

            int8_t resultAction = selling_update_result_menu(nowMs);

            if (resultAction == 0) {
                exit_state = MENU_STATE_HOME;
                break;
            }

            if (resultAction == 1) {
                exit_state = MENU_STATE_GAME_3;
                break;
            }
        }

        HAL_Delay(SELLING_LOOP_DELAY_MS);
    }

    selling_led_alert_stop();
    selling_buzzer_stop();
    PWM_SetDuty(&pwm_cfg, 0);

    while (selling_read_joy_sw_level()) {
        HAL_Delay(10);
    }

    HAL_Delay(200);

    return exit_state;
}
