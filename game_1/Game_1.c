#include "Game_1.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "LCD.h"
#include "Buzzer.h"
#include "PWM.h"
#include <stdio.h>

#define DIVER_MOVE_SPEED          3
#define DIVER_DASH_SPEED          8
#define DIVER_DASH_DURATION_MS    180
#define DIVER_DASH_COOLDOWN_MS    650
#define COUNTDOWN_DURATION_MS     3000
#define ATTACK_DURATION_MS        180
#define ATTACK_REACH_PX           18
#define REST_SPEAR_REACH_PX       8
#define CATCH_RESPAWN_DELAY_MS    450
#define CATCH_BEEP_FREQ_HZ        1100
#define CATCH_BEEP_MS             60
#define CATCH_BEEP_VOLUME         50
#define FISH_MIN_Y                40
#define FISH_MAX_Y                196
#define FISH_ROUTE_CHANGE_MIN_MS  260
#define FISH_ROUTE_CHANGE_VAR_MS  640
#define ROCK_ROUTE_CHANGE_MIN_MS  320
#define ROCK_ROUTE_CHANGE_VAR_MS  920
#define ROCK_MIN_Y                48
#define ROCK_MAX_Y                198
#define ROCK_MIN_X                20
#define ROCK_MAX_X                220
#define ROCK_PENALTY_MS           5000
#define ROCK_PENALTY_COOLDOWN_MS  900
#define PENALTY_MESSAGE_MS        550

#define COL_BLACK                 0
#define COL_WHITE                 1
#define COL_RED                   2
#define COL_GREEN                 3
#define COL_BLUE                  4
#define COL_ORANGE                5
#define COL_YELLOW                6
#define COL_PINK                  7
#define COL_PURPLE                8
#define COL_NAVY                  9
#define COL_GOLD                  10
#define COL_VIOLET                11
#define COL_BROWN                 12
#define COL_GREY                  13
#define COL_CYAN                  14
#define COL_MAGENTA               15

#define BG_TOP                    COL_CYAN
#define BG_MID                    COL_BLUE
#define BG_DEEP                   COL_NAVY
#define BG_PANEL                  COL_NAVY
#define BG_PANEL_ACCENT           COL_BLUE
#define BG_SAND                   COL_BROWN
#define BG_ROCK                   COL_GREY
#define BG_SEAWEED                COL_GREEN
#define HUD_TEXT                  COL_WHITE
#define MENU_TITLE_MAIN           COL_ORANGE
#define MENU_TITLE_SHADOW         COL_NAVY
#define MENU_SELECT               COL_YELLOW
#define MENU_TEXT                 COL_WHITE
#define RESULT_HIGHLIGHT          COL_YELLOW

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern Buzzer_cfg_t buzzer_cfg;
extern PWM_cfg_t pwm_cfg;

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint8_t volume;
} ToneStep_t;

static uint32_t buzzer_stop_tick = 0;
static uint32_t tone_step_end_tick = 0;
static const ToneStep_t* active_tone_seq = NULL;
static uint8_t active_tone_seq_len = 0;
static uint8_t active_tone_seq_index = 0;
static uint32_t led_flash_stop_tick = 0;
static uint8_t led_flash_duty = 0;

static const ToneStep_t k_start_tones[] = {
    {NOTE_C5,  70, 30},
    {0,        18,  0},
    {NOTE_E5,  70, 32},
    {0,        18,  0},
    {NOTE_G5,  85, 36},
    {0,        20,  0},
    {NOTE_C6, 130, 40}
};

static const ToneStep_t k_result_tones[] = {
    {NOTE_E6, 110, 38},
    {0,        20,  0},
    {NOTE_C6, 100, 36},
    {0,        20,  0},
    {NOTE_G5, 120, 34},
    {0,        22,  0},
    {NOTE_E5, 120, 34},
    {0,        24,  0},
    {NOTE_C5, 180, 42}
};

static const ToneStep_t k_penalty_tones[] = {
    {NOTE_A4, 60, 34},
    {0,       18,  0},
    {NOTE_F4, 90, 42}
};

static const FishType_t k_slot_types[FISHING_MAX_FISH] = {
    FISH_TYPE_BIG,
    FISH_TYPE_MEDIUM,
    FISH_TYPE_SMALL,
    FISH_TYPE_MEDIUM,
    FISH_TYPE_SMALL
};

static const int16_t k_slot_y[FISHING_MAX_FISH] = {
    52,
    78,
    104,
    136,
    166
};

static const int16_t k_initial_x[FISHING_MAX_FISH] = {
    20,
    172,
    132,
    210,
    64
};

static const int8_t k_initial_vx[FISHING_MAX_FISH] = {
    1,
    -2,
    3,
    -2,
    2
};

static const int8_t k_initial_vy[FISHING_MAX_FISH] = {
    1,
    -1,
    1,
    0,
    -1
};

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t button_edge(uint8_t current, uint8_t* latched)
{
    if (current) {
        if (!(*latched)) {
            *latched = 1;
            return 1;
        }
    } else {
        *latched = 0;
    }
    return 0;
}

static void safe_pixel(int16_t x, int16_t y, uint8_t colour)
{
    if (x >= 0 && x < FISHING_SCREEN_WIDTH && y >= 0 && y < FISHING_SCREEN_HEIGHT) {
        LCD_Set_Pixel((uint16_t)x, (uint16_t)y, colour);
    }
}

static void safe_hline(int16_t x0, int16_t x1, int16_t y, uint8_t colour)
{
    if (y < 0 || y >= FISHING_SCREEN_HEIGHT) {
        return;
    }
    if (x0 > x1) {
        int16_t t = x0;
        x0 = x1;
        x1 = t;
    }
    x0 = clamp_i16(x0, 0, FISHING_SCREEN_WIDTH - 1);
    x1 = clamp_i16(x1, 0, FISHING_SCREEN_WIDTH - 1);
    for (int16_t x = x0; x <= x1; ++x) {
        LCD_Set_Pixel((uint16_t)x, (uint16_t)y, colour);
    }
}

static void safe_vline(int16_t x, int16_t y0, int16_t y1, uint8_t colour)
{
    if (x < 0 || x >= FISHING_SCREEN_WIDTH) {
        return;
    }
    if (y0 > y1) {
        int16_t t = y0;
        y0 = y1;
        y1 = t;
    }
    y0 = clamp_i16(y0, 0, FISHING_SCREEN_HEIGHT - 1);
    y1 = clamp_i16(y1, 0, FISHING_SCREEN_HEIGHT - 1);
    for (int16_t y = y0; y <= y1; ++y) {
        LCD_Set_Pixel((uint16_t)x, (uint16_t)y, colour);
    }
}

static void safe_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour, uint8_t fill)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (fill) {
        for (int16_t yy = y; yy < y + h; ++yy) {
            safe_hline(x, x + w - 1, yy, colour);
        }
    } else {
        safe_hline(x, x + w - 1, y, colour);
        safe_hline(x, x + w - 1, y + h - 1, colour);
        safe_vline(x, y, y + h - 1, colour);
        safe_vline(x + w - 1, y, y + h - 1, colour);
    }
}

static void sprite_pixel(int16_t ox, int16_t oy, int16_t sprite_w, uint8_t flip_x,
                         int16_t px, int16_t py, uint8_t colour)
{
    int16_t sx = flip_x ? (ox + (sprite_w - 1 - px)) : (ox + px);
    safe_pixel(sx, oy + py, colour);
}

static void sprite_hline(int16_t ox, int16_t oy, int16_t sprite_w, uint8_t flip_x,
                         int16_t x0, int16_t x1, int16_t py, uint8_t colour)
{
    for (int16_t px = x0; px <= x1; ++px) {
        sprite_pixel(ox, oy, sprite_w, flip_x, px, py, colour);
    }
}

static void sprite_rect(int16_t ox, int16_t oy, int16_t sprite_w, uint8_t flip_x,
                        int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour)
{
    for (int16_t py = y; py < y + h; ++py) {
        sprite_hline(ox, oy, sprite_w, flip_x, x, x + w - 1, py, colour);
    }
}

static uint8_t triangle_wave_duty(uint32_t tick_ms, uint32_t period_ms, uint8_t min_duty, uint8_t max_duty)
{
    if (period_ms < 2u || max_duty <= min_duty) {
        return min_duty;
    }

    uint32_t phase = tick_ms % period_ms;
    uint32_t half_period = period_ms / 2u;
    uint32_t span = (uint32_t)(max_duty - min_duty);

    if (phase < half_period) {
        return (uint8_t)(min_duty + ((span * phase) / half_period));
    }

    return (uint8_t)(max_duty - ((span * (phase - half_period)) / half_period));
}

static void Feedback_SetLedDuty(uint8_t duty_percent)
{
    PWM_SetDuty(&pwm_cfg, duty_percent);
}

static void Feedback_FlashLed(uint8_t duty_percent, uint32_t duration_ms)
{
    led_flash_duty = duty_percent;
    led_flash_stop_tick = HAL_GetTick() + duration_ms;
    Feedback_SetLedDuty(duty_percent);
}

static void Feedback_PlayTone(uint32_t freq_hz, uint8_t volume_percent, uint32_t duration_ms)
{
    active_tone_seq = NULL;
    active_tone_seq_len = 0;
    active_tone_seq_index = 0;
    tone_step_end_tick = 0;

    if (freq_hz == 0 || volume_percent == 0 || duration_ms == 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_tick = 0;
        return;
    }

    buzzer_tone(&buzzer_cfg, freq_hz, volume_percent);
    buzzer_stop_tick = HAL_GetTick() + duration_ms;
}

static void Feedback_ApplyToneStep(uint8_t step_index)
{
    ToneStep_t step = active_tone_seq[step_index];

    if (step.freq_hz == 0 || step.volume == 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_tick = 0;
    } else {
        buzzer_tone(&buzzer_cfg, step.freq_hz, step.volume);
        buzzer_stop_tick = HAL_GetTick() + step.duration_ms;
    }

    tone_step_end_tick = HAL_GetTick() + step.duration_ms;
}

static void Feedback_StartToneSequence(const ToneStep_t* steps, uint8_t step_count)
{
    if (steps == NULL || step_count == 0) {
        active_tone_seq = NULL;
        active_tone_seq_len = 0;
        active_tone_seq_index = 0;
        tone_step_end_tick = 0;
        buzzer_off(&buzzer_cfg);
        buzzer_stop_tick = 0;
        return;
    }

    active_tone_seq = steps;
    active_tone_seq_len = step_count;
    active_tone_seq_index = 0;
    Feedback_ApplyToneStep(0);
}

static void Feedback_UpdateSound(void)
{
    uint32_t now = HAL_GetTick();

    if (active_tone_seq != NULL) {
        if ((int32_t)(now - tone_step_end_tick) >= 0) {
            active_tone_seq_index++;
            if (active_tone_seq_index >= active_tone_seq_len) {
                active_tone_seq = NULL;
                active_tone_seq_len = 0;
                active_tone_seq_index = 0;
                tone_step_end_tick = 0;
                buzzer_off(&buzzer_cfg);
                buzzer_stop_tick = 0;
            } else {
                Feedback_ApplyToneStep(active_tone_seq_index);
            }
        }
        return;
    }

    if (buzzer_stop_tick != 0 && (int32_t)(now - buzzer_stop_tick) >= 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_tick = 0;
    }
}

static void Feedback_UpdateLed(FishingState_t state, uint8_t attack_active, uint8_t dash_active)
{
    uint32_t now = HAL_GetTick();

    if (led_flash_stop_tick != 0 && (int32_t)(led_flash_stop_tick - now) > 0) {
        Feedback_SetLedDuty(led_flash_duty);
        return;
    }

    led_flash_stop_tick = 0;

    switch (state) {
        case FISHING_STATE_MENU:
            Feedback_SetLedDuty(triangle_wave_duty(now, 900, 3, 18));
            break;

        case FISHING_STATE_COUNTDOWN:
            Feedback_SetLedDuty(triangle_wave_duty(now, 260, 18, 55));
            break;

        case FISHING_STATE_PLAYING:
            if (dash_active) {
                Feedback_SetLedDuty(64);
            } else if (attack_active) {
                Feedback_SetLedDuty(26);
            } else {
                Feedback_SetLedDuty(triangle_wave_duty(now, 1200, 4, 10));
            }
            break;

        case FISHING_STATE_RESULTS:
            Feedback_SetLedDuty(triangle_wave_duty(now, 500, 8, 55));
            break;

        default:
            Feedback_SetLedDuty(0);
            break;
    }
}

static void Feedback_PlayCatch(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL:
            Feedback_FlashLed(28, 80);
            Feedback_PlayTone(NOTE_E6, 40, 40);
            break;

        case FISH_TYPE_MEDIUM:
            Feedback_FlashLed(45, 110);
            Feedback_PlayTone(NOTE_G5, 48, 55);
            break;

        case FISH_TYPE_BIG:
            Feedback_FlashLed(72, 150);
            Feedback_PlayTone(NOTE_C5, 58, 75);
            break;

        default:
            Feedback_FlashLed(32, 90);
            Feedback_PlayTone(CATCH_BEEP_FREQ_HZ, CATCH_BEEP_VOLUME, CATCH_BEEP_MS);
            break;
    }
}

static void Feedback_PlayAttack(void)
{
    Feedback_FlashLed(20, 45);
    Feedback_PlayTone(NOTE_A4, 20, 30);
}

static void Feedback_PlayStart(void)
{
    Feedback_FlashLed(55, 180);
    Feedback_StartToneSequence(k_start_tones, (uint8_t)(sizeof(k_start_tones) / sizeof(k_start_tones[0])));
}

static void Feedback_PlayResult(void)
{
    Feedback_FlashLed(80, 240);
    Feedback_StartToneSequence(k_result_tones, (uint8_t)(sizeof(k_result_tones) / sizeof(k_result_tones[0])));
}

static void Feedback_PlayPenalty(void)
{
    Feedback_FlashLed(88, 180);
    Feedback_StartToneSequence(k_penalty_tones, (uint8_t)(sizeof(k_penalty_tones) / sizeof(k_penalty_tones[0])));
}

static uint8_t fish_width(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL: return 18;
        case FISH_TYPE_MEDIUM: return 22;
        case FISH_TYPE_BIG: return 28;
        default: return 18;
    }
}

static uint8_t fish_height(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL: return 10;
        case FISH_TYPE_MEDIUM: return 12;
        case FISH_TYPE_BIG: return 16;
        default: return 10;
    }
}

static int8_t fish_speed(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL: return 3;
        case FISH_TYPE_MEDIUM: return 2;
        case FISH_TYPE_BIG: return 1;
        default: return 2;
    }
}

static uint16_t fish_value(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL: return 40;
        case FISH_TYPE_MEDIUM: return 25;
        case FISH_TYPE_BIG: return 12;
        default: return 0;
    }
}

static uint32_t next_route_change_tick(void)
{
    return HAL_GetTick() + FISH_ROUTE_CHANGE_MIN_MS + Random_U16(FISH_ROUTE_CHANGE_VAR_MS);
}

static int8_t random_vertical_speed(FishType_t type)
{
    int8_t vy = 0;

    switch (type) {
        case FISH_TYPE_SMALL:
            vy = (int8_t)Random_U16(5) - 2;
            break;
        case FISH_TYPE_MEDIUM:
            vy = (int8_t)Random_U16(5) - 2;
            break;
        case FISH_TYPE_BIG:
        default:
            vy = (int8_t)Random_U16(3) - 1;
            break;
    }

    return vy;
}

static int8_t random_horizontal_speed(FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL: return (int8_t)(3 + Random_U16(2));
        case FISH_TYPE_MEDIUM: return (int8_t)(2 + Random_U16(2));
        case FISH_TYPE_BIG: return (int8_t)(1 + Random_U16(2));
        default: return 2;
    }
}

static uint32_t next_rock_route_change_tick(void)
{
    return HAL_GetTick() + ROCK_ROUTE_CHANGE_MIN_MS + Random_U16(ROCK_ROUTE_CHANGE_VAR_MS);
}

static int8_t random_rock_speed(void)
{
    int8_t magnitude = (int8_t)(1 + Random_U16(2));
    return (Random_U16(2) == 0) ? magnitude : (int8_t)-magnitude;
}

static void setup_rock(Rock_t* rock, uint8_t slot)
{
    static const int16_t k_rock_seed_x[FISHING_MAX_ROCKS] = {96, 142, 176, 120};
    static const int16_t k_rock_seed_y[FISHING_MAX_ROCKS] = {64, 108, 150, 184};
    uint8_t base = (uint8_t)(10 + Random_U16(4));

    rock->width = (int16_t)(base + (slot & 1));
    rock->height = (int16_t)(base - 2 + (slot % 3));
    rock->x = (int16_t)(k_rock_seed_x[slot] + (int16_t)Random_U16(30) - 15);
    rock->y = (int16_t)(k_rock_seed_y[slot] + (int16_t)Random_U16(20) - 10);
    rock->vx = random_rock_speed();
    rock->vy = random_rock_speed();
    rock->route_change_tick = next_rock_route_change_tick();
}

static void reset_all_rocks(FishingEngine_t* engine)
{
    for (uint8_t i = 0; i < FISHING_MAX_ROCKS; ++i) {
        setup_rock(&engine->rocks[i], i);
    }
}

static void setup_fish_for_slot(Fish_t* fish, uint8_t slot)
{
    FishType_t type = k_slot_types[slot];
    int16_t center_y = k_slot_y[slot];
    int16_t spread = (type == FISH_TYPE_BIG) ? 8 : 12;

    fish->type = type;
    fish->width = fish_width(type);
    fish->height = fish_height(type);
    fish->y = clamp_i16((int16_t)(center_y + (int16_t)Random_U16((uint16_t)(2 * spread + 1)) - spread),
                        FISH_MIN_Y,
                        (int16_t)(FISH_MAX_Y - fish->height));
    fish->vy = random_vertical_speed(type);
    fish->active = 1;
    fish->respawn_tick = 0;
    fish->route_change_tick = next_route_change_tick();
}

static void spawn_fish_in_slot(Fish_t* fish, uint8_t slot, uint8_t from_left)
{
    int8_t speed = 0;

    setup_fish_for_slot(fish, slot);
    speed = random_horizontal_speed(fish->type);
    if (from_left) {
        fish->x = -fish->width - (int16_t)Random_U16(18);
        fish->vx = speed;
    } else {
        fish->x = FISHING_SCREEN_WIDTH + (int16_t)Random_U16(18);
        fish->vx = -speed;
    }
}

static void reset_all_fish(FishingEngine_t* engine)
{
    for (uint8_t i = 0; i < FISHING_MAX_FISH; ++i) {
        setup_fish_for_slot(&engine->fish[i], i);
        engine->fish[i].x = k_initial_x[i];
        engine->fish[i].vx = k_initial_vx[i];
        engine->fish[i].vy = k_initial_vy[i];
        engine->fish[i].route_change_tick = next_route_change_tick();
    }
}

static AABB get_spear_box(const Diver_t* diver)
{
    AABB box;
    box.width = ATTACK_REACH_PX;
    box.height = 6;
    box.y = diver->y + 10;
    if (diver->facing_right) {
        box.x = diver->x + diver->width - 2;
    } else {
        box.x = diver->x - ATTACK_REACH_PX + 2;
    }
    return box;
}

static AABB get_fish_box(const Fish_t* fish)
{
    AABB box;
    box.x = fish->x + 2;
    box.y = fish->y + 2;
    box.width = fish->width - 6;
    box.height = fish->height - 4;
    return box;
}

static AABB get_diver_box(const Diver_t* diver)
{
    AABB box;
    box.x = diver->x + 3;
    box.y = diver->y + 2;
    box.width = diver->width - 6;
    box.height = diver->height - 2;
    return box;
}

static AABB get_rock_box(const Rock_t* rock)
{
    AABB box;
    box.x = rock->x + 1;
    box.y = rock->y + 1;
    box.width = rock->width - 2;
    box.height = rock->height - 2;
    return box;
}

static void diver_init(Diver_t* diver)
{
    diver->x = 48;
    diver->y = 124;
    diver->width = 22;
    diver->height = 22;
    diver->facing_right = 1;
    diver->attack_active = 0;
    diver->attack_end_tick = 0;
    diver->dash_active = 0;
    diver->dash_end_tick = 0;
    diver->dash_cooldown_tick = 0;
    diver->dash_dx = 1;
    diver->dash_dy = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        diver->trail_x[i] = diver->x;
        diver->trail_y[i] = diver->y;
    }
}

void FishingEngine_Init(FishingEngine_t* engine)
{
    engine->state = FISHING_STATE_MENU;
    engine->selected_menu_index = 0;
    engine->menu_confirm_latched = 0;
    engine->result_confirm_latched = 0;
    engine->attack_latched = 0;
    engine->dash_latched = 0;
    engine->score = 0;
    engine->caught_small = 0;
    engine->caught_medium = 0;
    engine->caught_big = 0;
    engine->game_start_tick = HAL_GetTick();
    engine->countdown_start_tick = 0;
    engine->game_duration_ms = 120000;
    engine->time_penalty_ms = 0;
    engine->countdown_last_number = 0;
    engine->penalty_cooldown_tick = 0;
    engine->penalty_message_tick = 0;
    engine->finished = 0;
    engine->next_action = FISHING_NEXT_MAIN_MENU;
    diver_init(&engine->diver);
    reset_all_fish(engine);
    reset_all_rocks(engine);
}

void FishingEngine_StartGame(FishingEngine_t* engine)
{
    engine->state = FISHING_STATE_COUNTDOWN;
    engine->finished = 0;
    engine->next_action = FISHING_NEXT_MAIN_MENU;
    engine->selected_menu_index = 0;
    engine->result_confirm_latched = 0;
    engine->score = 0;
    engine->caught_small = 0;
    engine->caught_medium = 0;
    engine->caught_big = 0;
    engine->attack_latched = 0;
    engine->dash_latched = 0;
    engine->time_penalty_ms = 0;
    engine->penalty_cooldown_tick = 0;
    engine->penalty_message_tick = 0;
    diver_init(&engine->diver);
    reset_all_fish(engine);
    reset_all_rocks(engine);
    engine->countdown_start_tick = HAL_GetTick();
    engine->countdown_last_number = 0;
    engine->game_start_tick = engine->countdown_start_tick;
    Feedback_PlayTone(NOTE_C5, 36, 80);
}

static void update_menu(FishingEngine_t* engine, uint8_t attack_pressed)
{
    uint32_t now = HAL_GetTick();
    if ((now - engine->game_start_tick) >= 1200u || attack_pressed) {
        FishingEngine_StartGame(engine);
    }
}

static void direction_to_delta(Direction direction, int8_t* dx, int8_t* dy)
{
    *dx = 0;
    *dy = 0;

    switch (direction) {
        case N:  *dy = -1; break;
        case S:  *dy =  1; break;
        case E:  *dx =  1; break;
        case W:  *dx = -1; break;
        case NE: *dx =  1; *dy = -1; break;
        case NW: *dx = -1; *dy = -1; break;
        case SE: *dx =  1; *dy =  1; break;
        case SW: *dx = -1; *dy =  1; break;
        default: break;
    }
}

static void diver_save_trail(Diver_t* diver)
{
    diver->trail_x[2] = diver->trail_x[1];
    diver->trail_x[1] = diver->trail_x[0];
    diver->trail_x[0] = diver->x;

    diver->trail_y[2] = diver->trail_y[1];
    diver->trail_y[1] = diver->trail_y[0];
    diver->trail_y[0] = diver->y;
}

static void Feedback_PlayDash(void)
{
    Feedback_FlashLed(68, 90);
    Feedback_PlayTone(NOTE_E6, 34, 45);
}

static void update_countdown(FishingEngine_t* engine)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - engine->countdown_start_tick;

    if (elapsed >= COUNTDOWN_DURATION_MS) {
        engine->state = FISHING_STATE_PLAYING;
        engine->game_start_tick = now;
        engine->countdown_last_number = 0;
        Feedback_PlayStart();
        return;
    }

    uint8_t number = (uint8_t)(3u - (elapsed / 1000u));
    if (number == 0) {
        number = 1;
    }

    if (number != engine->countdown_last_number) {
        engine->countdown_last_number = number;
        Feedback_PlayTone((number == 1u) ? NOTE_G5 : NOTE_C5, 34, 75);
    }
}

static void update_diver(FishingEngine_t* engine, UserInput input, uint8_t dash_pressed)
{
    Diver_t* diver = &engine->diver;
    uint32_t now = HAL_GetTick();
    int8_t move_dx = 0;
    int8_t move_dy = 0;

    diver_save_trail(diver);
    direction_to_delta(input.direction, &move_dx, &move_dy);

    uint8_t dash_edge = button_edge(dash_pressed, &engine->dash_latched);
    if (dash_edge && !diver->dash_active && (int32_t)(now - diver->dash_cooldown_tick) >= 0) {
        if (move_dx == 0 && move_dy == 0) {
            move_dx = diver->facing_right ? 1 : -1;
        }

        diver->dash_dx = move_dx;
        diver->dash_dy = move_dy;
        diver->dash_active = 1;
        diver->dash_end_tick = now + DIVER_DASH_DURATION_MS;
        diver->dash_cooldown_tick = now + DIVER_DASH_COOLDOWN_MS;

        if (diver->dash_dx > 0) {
            diver->facing_right = 1;
        } else if (diver->dash_dx < 0) {
            diver->facing_right = 0;
        }

        Feedback_PlayDash();
    }

    if (diver->dash_active) {
        int16_t dash_step_x = (int16_t)diver->dash_dx * DIVER_DASH_SPEED;
        int16_t dash_step_y = (int16_t)diver->dash_dy * DIVER_DASH_SPEED;

        if (diver->dash_dx != 0 && diver->dash_dy != 0) {
            dash_step_x = (int16_t)diver->dash_dx * ((DIVER_DASH_SPEED * 7) / 10);
            dash_step_y = (int16_t)diver->dash_dy * ((DIVER_DASH_SPEED * 7) / 10);
        }

        diver->x += dash_step_x;
        diver->y += dash_step_y;

        if ((int32_t)(now - diver->dash_end_tick) >= 0) {
            diver->dash_active = 0;
        }
    } else {
        diver->x += (int16_t)move_dx * DIVER_MOVE_SPEED;
        diver->y += (int16_t)move_dy * DIVER_MOVE_SPEED;

        if (move_dx > 0) {
            diver->facing_right = 1;
        } else if (move_dx < 0) {
            diver->facing_right = 0;
        }
    }

    diver->x = clamp_i16(diver->x, 20, FISHING_SCREEN_WIDTH - diver->width - 20);
    diver->y = clamp_i16(diver->y, 36, FISHING_SCREEN_HEIGHT - diver->height - 20);
}

static void update_attack(FishingEngine_t* engine, uint8_t attack_pressed)
{
    Diver_t* diver = &engine->diver;
    uint8_t pressed_edge = button_edge(attack_pressed, &engine->attack_latched);

    if (pressed_edge && !diver->attack_active) {
        diver->attack_active = 1;
        diver->attack_end_tick = HAL_GetTick() + ATTACK_DURATION_MS;
        Feedback_PlayAttack();
    }

    if (diver->attack_active && (int32_t)(HAL_GetTick() - diver->attack_end_tick) >= 0) {
        diver->attack_active = 0;
    }
}

static void update_fish(FishingEngine_t* engine)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < FISHING_MAX_FISH; ++i) {
        Fish_t* fish = &engine->fish[i];

        if (!fish->active) {
            if ((int32_t)(now - fish->respawn_tick) >= 0) {
                spawn_fish_in_slot(fish, i, (uint8_t)Random_U16(2));
            }
            continue;
        }

        if ((int32_t)(now - fish->route_change_tick) >= 0) {
            int8_t sign = (fish->vx >= 0) ? 1 : -1;
            fish->vy = random_vertical_speed(fish->type);
            fish->vx = (int8_t)(sign * random_horizontal_speed(fish->type));
            fish->route_change_tick = next_route_change_tick();
        }

        fish->x += fish->vx;
        fish->y += fish->vy;

        if (fish->y < FISH_MIN_Y) {
            fish->y = FISH_MIN_Y;
            fish->vy = (int8_t)(1 + Random_U16(2));
            fish->route_change_tick = next_route_change_tick();
        } else if ((fish->y + fish->height) > FISH_MAX_Y) {
            fish->y = FISH_MAX_Y - fish->height;
            fish->vy = (int8_t)-(1 + Random_U16(2));
            fish->route_change_tick = next_route_change_tick();
        }

        if (fish->vx > 0 && fish->x > (FISHING_SCREEN_WIDTH + fish->width + 18)) {
            spawn_fish_in_slot(fish, i, 0);
        } else if (fish->vx < 0 && (fish->x + fish->width) < -18) {
            spawn_fish_in_slot(fish, i, 1);
        }
    }
}

static void update_rocks(FishingEngine_t* engine)
{
    for (uint8_t i = 0; i < FISHING_MAX_ROCKS; ++i) {
        Rock_t* rock = &engine->rocks[i];

        if ((int32_t)(HAL_GetTick() - rock->route_change_tick) >= 0) {
            if (Random_U16(3) != 0) {
                rock->vx = random_rock_speed();
            }
            if (Random_U16(3) != 0) {
                rock->vy = random_rock_speed();
            }
            rock->route_change_tick = next_rock_route_change_tick();
        }

        rock->x += rock->vx;
        rock->y += rock->vy;

        if (rock->x < ROCK_MIN_X) {
            rock->x = ROCK_MIN_X;
            rock->vx = (int8_t)(1 + Random_U16(2));
            rock->route_change_tick = next_rock_route_change_tick();
        } else if ((rock->x + rock->width) > ROCK_MAX_X) {
            rock->x = ROCK_MAX_X - rock->width;
            rock->vx = (int8_t)-(1 + Random_U16(2));
            rock->route_change_tick = next_rock_route_change_tick();
        }

        if (rock->y < ROCK_MIN_Y) {
            rock->y = ROCK_MIN_Y;
            rock->vy = (int8_t)(1 + Random_U16(2));
            rock->route_change_tick = next_rock_route_change_tick();
        } else if ((rock->y + rock->height) > ROCK_MAX_Y) {
            rock->y = ROCK_MAX_Y - rock->height;
            rock->vy = (int8_t)-(1 + Random_U16(2));
            rock->route_change_tick = next_rock_route_change_tick();
        }
    }
}

static void apply_catch_stats(FishingEngine_t* engine, FishType_t type)
{
    switch (type) {
        case FISH_TYPE_SMALL:
            engine->caught_small++;
            break;
        case FISH_TYPE_MEDIUM:
            engine->caught_medium++;
            break;
        case FISH_TYPE_BIG:
            engine->caught_big++;
            break;
        default:
            break;
    }
}

static void check_collisions(FishingEngine_t* engine)
{
    if (!engine->diver.attack_active) {
        return;
    }

    AABB spear_box = get_spear_box(&engine->diver);

    for (uint8_t i = 0; i < FISHING_MAX_FISH; ++i) {
        Fish_t* fish = &engine->fish[i];
        if (!fish->active) {
            continue;
        }

        AABB fish_box = get_fish_box(fish);
        if (AABB_Collides(&spear_box, &fish_box)) {
            engine->score += fish_value(fish->type);
            apply_catch_stats(engine, fish->type);
            fish->active = 0;
            fish->respawn_tick = HAL_GetTick() + CATCH_RESPAWN_DELAY_MS;
            Feedback_PlayCatch(fish->type);
            break;
        }
    }
}

static void check_rock_collisions(FishingEngine_t* engine)
{
    uint32_t now = HAL_GetTick();

    if ((int32_t)(engine->penalty_cooldown_tick - now) > 0) {
        return;
    }

    AABB diver_box = get_diver_box(&engine->diver);

    for (uint8_t i = 0; i < FISHING_MAX_ROCKS; ++i) {
        Rock_t* rock = &engine->rocks[i];
        AABB rock_box = get_rock_box(rock);

        if (AABB_Collides(&diver_box, &rock_box)) {
            if (engine->time_penalty_ms + ROCK_PENALTY_MS >= engine->game_duration_ms) {
                engine->time_penalty_ms = engine->game_duration_ms;
            } else {
                engine->time_penalty_ms += ROCK_PENALTY_MS;
            }
            engine->penalty_cooldown_tick = now + ROCK_PENALTY_COOLDOWN_MS;
            engine->penalty_message_tick = now + PENALTY_MESSAGE_MS;

            if (rock->vx >= 0) {
                engine->diver.x = clamp_i16((int16_t)(engine->diver.x - 10), 20, FISHING_SCREEN_WIDTH - engine->diver.width - 20);
            } else {
                engine->diver.x = clamp_i16((int16_t)(engine->diver.x + 10), 20, FISHING_SCREEN_WIDTH - engine->diver.width - 20);
            }
            Feedback_PlayPenalty();
            break;
        }
    }
}

static void update_results(FishingEngine_t* engine, UserInput input, uint8_t attack_pressed, uint8_t back_pressed)
{
    if (input.direction == N || input.direction == S || input.direction == W || input.direction == E) {
        if (engine->menu_confirm_latched == 0) {
            engine->selected_menu_index = (engine->selected_menu_index == 0) ? 1u : 0u;
            engine->menu_confirm_latched = 1;
        }
    } else {
        engine->menu_confirm_latched = 0;
    }

    uint8_t confirm = attack_pressed || back_pressed;
    if (button_edge(confirm, &engine->result_confirm_latched)) {
        engine->next_action = (engine->selected_menu_index == 0) ? FISHING_NEXT_MAIN_MENU : FISHING_NEXT_COOKING;
        engine->finished = 1;
    }
}

void FishingEngine_Update(FishingEngine_t* engine, UserInput input, uint8_t attack_pressed, uint8_t dash_pressed, uint8_t back_pressed)
{
    Feedback_UpdateSound();

    switch (engine->state) {
        case FISHING_STATE_MENU:
            update_menu(engine, attack_pressed);
            break;

        case FISHING_STATE_COUNTDOWN:
            update_countdown(engine);
            break;

        case FISHING_STATE_PLAYING:
            if (back_pressed) {
                engine->state = FISHING_STATE_MENU;
                engine->game_start_tick = HAL_GetTick();
                break;
            }
            update_diver(engine, input, dash_pressed);
            update_attack(engine, attack_pressed);
            update_fish(engine);
            update_rocks(engine);
            check_collisions(engine);
            check_rock_collisions(engine);
            if (FishingEngine_GetTimeRemainingMs(engine) == 0) {
                engine->state = FISHING_STATE_RESULTS;
                engine->diver.attack_active = 0;
                engine->diver.dash_active = 0;
                Feedback_PlayResult();
            }
            break;

        case FISHING_STATE_RESULTS:
            update_results(engine, input, attack_pressed, back_pressed);
            break;

        default:
            engine->state = FISHING_STATE_MENU;
            engine->game_start_tick = HAL_GetTick();
            break;
    }

    Feedback_UpdateLed(engine->state, engine->diver.attack_active, engine->diver.dash_active);
}


static void draw_ocean_backdrop(void)
{
    LCD_Fill_Buffer(BG_DEEP);
    LCD_Draw_Rect(0, 0, FISHING_SCREEN_WIDTH, 34, BG_TOP, 1);
    LCD_Draw_Rect(0, 34, FISHING_SCREEN_WIDTH, 48, BG_MID, 1);
    LCD_Draw_Rect(0, 82, FISHING_SCREEN_WIDTH, 138, BG_DEEP, 1);
    LCD_Draw_Rect(0, 220, FISHING_SCREEN_WIDTH, 20, BG_SAND, 1);

    for (uint8_t i = 0; i < 5; ++i) {
        LCD_Draw_Line(0, 38 + i * 2, FISHING_SCREEN_WIDTH - 1, 38 + i * 2, BG_TOP);
    }
    for (uint8_t i = 0; i < 6; ++i) {
        LCD_Draw_Line(0, 82 + i * 2, FISHING_SCREEN_WIDTH - 1, 82 + i * 2, BG_MID);
    }

    safe_rect(0, 0, FISHING_SCREEN_WIDTH, 22, COL_NAVY, 1);
    safe_rect(0, 22, FISHING_SCREEN_WIDTH, 2, COL_BLUE, 1);

    for (int16_t x = 10; x < 220; x += 24) {
        safe_pixel(x, 60, BG_TOP);
        safe_pixel(x + 2, 62, BG_TOP);
        safe_pixel(x + 4, 60, BG_TOP);
    }
}

static void draw_rocks(void)
{
    const int16_t rock_y = 208;
    for (int16_t x = 0; x < FISHING_SCREEN_WIDTH; x += 18) {
        int16_t w = 14 + (x % 3);
        int16_t h = 8 + ((x / 18) % 5);
        safe_rect(x, rock_y - h, w, h, BG_ROCK, 1);
        safe_hline(x + 2, x + w - 3, rock_y - h + 2, COL_WHITE);
    }
}

static void draw_seaweed_cluster(int16_t x, int16_t base_y, int16_t height)
{
    for (int16_t i = 0; i < 4; ++i) {
        int16_t stem_x = x + i * 3;
        int16_t sway = (i & 1) ? 3 : -3;
        for (int16_t y = 0; y < height; ++y) {
            int16_t px = stem_x + (y / 6) * sway;
            safe_pixel(px, base_y - y, BG_SEAWEED);
            if ((y % 7) == 0) {
                safe_pixel(px + ((sway > 0) ? 1 : -1), base_y - y, COL_YELLOW);
            }
        }
    }
}

static void draw_bubbles(int16_t x, int16_t y_start, int16_t count)
{
    for (int16_t i = 0; i < count; ++i) {
        int16_t bx = x + ((i & 1) ? 4 : 0);
        int16_t by = y_start - i * 10;
        safe_rect(bx, by, 3, 3, COL_CYAN, 1);
        safe_pixel(bx, by, COL_WHITE);
    }
}

static void draw_small_crab_icon(int16_t x, int16_t y)
{
    safe_rect(x + 4, y + 4, 10, 6, COL_RED, 1);
    safe_rect(x + 6, y + 2, 2, 2, COL_RED, 1);
    safe_rect(x + 10, y + 2, 2, 2, COL_RED, 1);
    safe_pixel(x + 5, y + 5, COL_WHITE);
    safe_pixel(x + 11, y + 5, COL_WHITE);
    safe_pixel(x + 5, y + 1, COL_RED);
    safe_pixel(x + 13, y + 1, COL_RED);
    safe_vline(x + 3, y + 10, y + 13, COL_RED);
    safe_vline(x + 7, y + 10, y + 13, COL_RED);
    safe_vline(x + 11, y + 10, y + 13, COL_RED);
    safe_vline(x + 15, y + 10, y + 13, COL_RED);
}

static void draw_panel(int16_t x, int16_t y, int16_t w, int16_t h)
{
    safe_rect(x, y, w, h, BG_PANEL_ACCENT, 1);
    safe_rect(x + 3, y + 3, w - 6, h - 6, BG_PANEL, 1);
    safe_rect(x, y, w, h, COL_WHITE, 0);
    safe_rect(x + 2, y + 2, w - 4, h - 4, COL_CYAN, 0);
}

static void draw_penalty_popup(void)
{
    draw_panel(84, 94, 72, 34);
    safe_rect(92, 100, 56, 4, COL_RED, 1);
    LCD_printString("-5s", 98, 108, COL_YELLOW, 2);
}

static void draw_fish_body(const Fish_t* fish)
{
    int16_t x = fish->x;
    int16_t y = fish->y;
    int16_t w = fish->width;
    int16_t h = fish->height;
    uint8_t flip_x = (fish->vx < 0) ? 1u : 0u;
    uint8_t base = COL_GREEN;
    uint8_t stripe = COL_YELLOW;
    uint8_t belly = COL_WHITE;
    uint8_t fin = COL_GREEN;

    switch (fish->type) {
        case FISH_TYPE_SMALL:
            base = COL_GREEN;
            stripe = COL_YELLOW;
            belly = COL_WHITE;
            fin = COL_GREEN;
            break;
        case FISH_TYPE_MEDIUM:
            base = COL_PURPLE;
            stripe = COL_VIOLET;
            belly = COL_WHITE;
            fin = COL_PURPLE;
            break;
        case FISH_TYPE_BIG:
            base = COL_GREY;
            stripe = COL_GREY;
            belly = COL_WHITE;
            fin = COL_GREY;
            break;
        default:
            break;
    }

    for (int16_t py = 0; py < h; ++py) {
        int16_t inset = 0;
        if (py < 2 || py >= h - 2) inset = 5;
        else if (py < 4 || py >= h - 4) inset = 3;
        else if (py == h / 2) inset = 0;
        else inset = 1;

        int16_t start = 4 + inset;
        int16_t end = w - 8 - inset;
        sprite_hline(x, y, w, flip_x, start, end, py, base);
        if (py >= h / 2) {
            sprite_hline(x, y, w, flip_x, start + 1, end - 1, py, belly);
        }
    }

    for (int16_t py = 2; py < h - 2; ++py) {
        int16_t nose_w = (py <= h / 2) ? (py - 1) : ((h - 2) - py);
        if (nose_w < 0) nose_w = 0;
        sprite_hline(x, y, w, flip_x, 2, 3 + nose_w, py, base);
    }

    for (int16_t py = 1; py < h - 1; ++py) {
        int16_t tail_w = (py <= h / 2) ? py : (h - 1 - py);
        if (tail_w < 0) tail_w = 0;
        sprite_hline(x, y, w, flip_x, w - 7, w - 7 + tail_w, py, fin);
    }

    sprite_hline(x, y, w, flip_x, 8, 12, 1, fin);
    sprite_hline(x, y, w, flip_x, 9, 13, 0, fin);
    sprite_hline(x, y, w, flip_x, 10, 13, h - 2, fin);

    if (fish->type == FISH_TYPE_SMALL) {
        for (int16_t sx = 8; sx <= 14; sx += 3) {
            for (int16_t py = 2; py < h - 2; ++py) {
                sprite_pixel(x, y, w, flip_x, sx, py, stripe);
            }
        }
    } else if (fish->type == FISH_TYPE_MEDIUM) {
        for (int16_t sx = 7; sx <= 13; sx += 3) {
            for (int16_t py = 2; py < h - 2; ++py) {
                sprite_pixel(x, y, w, flip_x, sx, py, stripe);
            }
        }
    } else {
        for (int16_t sx = 7; sx <= 14; sx += 4) {
            for (int16_t py = 3; py < h - 3; ++py) {
                if (((py + sx) & 1) == 0) {
                    sprite_pixel(x, y, w, flip_x, sx, py, COL_WHITE);
                }
            }
        }
    }

    sprite_rect(x, y, w, flip_x, 5, 4, 2, 2, COL_WHITE);
    sprite_pixel(x, y, w, flip_x, 6, 5, COL_BLACK);
    sprite_pixel(x, y, w, flip_x, 4, 7, COL_BLACK);
    sprite_pixel(x, y, w, flip_x, 5, 8, COL_BLACK);

    safe_hline(x + 4, x + w - 9, y + h - 1, COL_BLACK);
    safe_hline(x + 5, x + w - 9, y, COL_BLACK);
}

static void draw_dash_trail(const Diver_t* diver)
{
    if (!diver->dash_active) {
        return;
    }

    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t colour = (i == 0u) ? COL_CYAN : ((i == 1u) ? COL_BLUE : COL_WHITE);
        int16_t tx = diver->trail_x[i] + 10;
        int16_t ty = diver->trail_y[i] + 10;
        safe_rect(tx - 2, ty - 2, 4, 4, colour, 1);
        safe_pixel(tx + 4, ty - 3, COL_WHITE);
        safe_pixel(tx - 5, ty + 3, COL_WHITE);
    }

    int16_t line_x0 = diver->facing_right ? (diver->x - 16) : (diver->x + diver->width + 8);
    int16_t line_x1 = diver->facing_right ? (diver->x - 4) : (diver->x + diver->width + 20);
    safe_hline(line_x0, line_x1, diver->y + 7, COL_CYAN);
    safe_hline(line_x0 - 4, line_x1 - 2, diver->y + 14, COL_WHITE);
}

static void draw_diver(const Diver_t* diver)
{
    int16_t x = diver->x;
    int16_t y = diver->y;
    int16_t w = diver->width;
    uint8_t flip_x = diver->facing_right ? 0u : 1u;
    uint8_t spear_len = diver->attack_active ? ATTACK_REACH_PX : REST_SPEAR_REACH_PX;

    sprite_rect(x, y, w, flip_x, 1, 7, 4, 8, COL_YELLOW);
    sprite_rect(x, y, w, flip_x, 2, 8, 2, 6, COL_GOLD);

    sprite_rect(x, y, w, flip_x, 5, 7, 10, 9, COL_NAVY);
    sprite_rect(x, y, w, flip_x, 7, 16, 3, 4, COL_NAVY);
    sprite_rect(x, y, w, flip_x, 11, 16, 3, 4, COL_NAVY);

    sprite_rect(x, y, w, flip_x, 5, 2, 10, 6, COL_BLACK);
    sprite_rect(x, y, w, flip_x, 6, 3, 8, 4, COL_GREY);
    sprite_rect(x, y, w, flip_x, 7, 3, 6, 4, COL_CYAN);
    sprite_rect(x, y, w, flip_x, 8, 7, 4, 2, COL_ORANGE);

    sprite_rect(x, y, w, flip_x, 14, 1, 2, 5, COL_NAVY);
    sprite_rect(x, y, w, flip_x, 14, 1, 1, 1, COL_CYAN);
    sprite_rect(x, y, w, flip_x, 13, 5, 3, 1, COL_YELLOW);

    sprite_rect(x, y, w, flip_x, 5, 10, 2, 4, COL_YELLOW);
    sprite_rect(x, y, w, flip_x, 13, 10, 2, 4, COL_ORANGE);
    sprite_rect(x, y, w, flip_x, 14, 10, 2, 2, COL_YELLOW);

    sprite_rect(x, y, w, flip_x, 6, 20, 4, 2, COL_YELLOW);
    sprite_rect(x, y, w, flip_x, 11, 20, 4, 2, COL_YELLOW);

    safe_rect(diver->facing_right ? (x + 15) : (x - spear_len + 1), y + 10, spear_len, 2, COL_WHITE, 1);
    safe_rect(diver->facing_right ? (x + 15 + spear_len - 1) : (x - spear_len - 2), y + 9, 3, 4, COL_GREY, 1);
    if (diver->facing_right) {
        safe_pixel(x + 15 + spear_len + 1, y + 10, COL_WHITE);
        safe_pixel(x + 15 + spear_len + 1, y + 11, COL_WHITE);
    } else {
        safe_pixel(x - spear_len - 3, y + 10, COL_WHITE);
        safe_pixel(x - spear_len - 3, y + 11, COL_WHITE);
    }
}

static void draw_menu(FishingEngine_t* engine)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - engine->game_start_tick;
    uint32_t phase = (now / 180u) % 4u;
    uint16_t progress = (uint16_t)(elapsed * 140u / 1200u);
    if (progress > 140u) {
        progress = 140u;
    }

    LCD_Fill_Buffer(BG_DEEP);
    LCD_Draw_Rect(0, 0, FISHING_SCREEN_WIDTH, 72, BG_TOP, 1);
    LCD_Draw_Rect(0, 72, FISHING_SCREEN_WIDTH, 58, BG_MID, 1);
    LCD_Draw_Rect(0, 130, FISHING_SCREEN_WIDTH, 110, BG_DEEP, 1);
    LCD_Draw_Rect(0, 86, FISHING_SCREEN_WIDTH, 3, COL_WHITE, 1);

    LCD_printString("FISHING", 48, 36, COL_WHITE, 3);
    LCD_printString("LOADING", 62, 102, COL_YELLOW, 2);

    if (phase == 0u) {
        LCD_printString(".", 156, 102, COL_YELLOW, 2);
    } else if (phase == 1u) {
        LCD_printString("..", 156, 102, COL_YELLOW, 2);
    } else {
        LCD_printString("...", 156, 102, COL_YELLOW, 2);
    }

    draw_panel(38, 138, 164, 34);
    LCD_Draw_Rect(50, 151, 140, 8, COL_WHITE, 0);
    LCD_Draw_Rect(50, 151, progress, 8, COL_YELLOW, 1);

    draw_bubbles(24, 190, 7);
    draw_seaweed_cluster(10, 220, 42);
    draw_seaweed_cluster(208, 220, 35);
    draw_rocks();
    draw_small_crab_icon(190, 28);

    LCD_printString("Get ready to dive", 54, 190, COL_WHITE, 1);
}

static void draw_play_background(void)
{
    draw_ocean_backdrop();
    draw_bubbles(18, 180, 9);
    draw_seaweed_cluster(8, 220, 46);
    draw_seaweed_cluster(212, 220, 40);
    draw_rocks();
    safe_rect(92, 168, 22, 12, BG_MID, 1);
    safe_rect(104, 160, 6, 8, BG_MID, 1);
}

static void draw_fish(const Fish_t* fish)
{
    if (!fish->active) {
        return;
    }
    draw_fish_body(fish);
}

static void draw_hazard_rock(const Rock_t* rock)
{
    int16_t x = rock->x;
    int16_t y = rock->y;
    int16_t w = rock->width;
    int16_t h = rock->height;

    safe_rect(x, y + 2, w, h - 2, BG_ROCK, 1);
    safe_rect(x + 1, y + 1, w - 2, h - 3, COL_GREY, 1);
    safe_hline(x + 2, x + w - 3, y + 2, COL_WHITE);
    safe_pixel(x + 3, y + h / 2, COL_WHITE);
    safe_pixel(x + w - 4, y + h / 2 + 1, COL_BLACK);
    safe_pixel(x + 2, y + h - 2, COL_BLACK);
    safe_pixel(x + w - 3, y + h - 3, COL_BLACK);
}

static void draw_hud(FishingEngine_t* engine)
{
    char text[32];
    uint32_t time_left_s = (FishingEngine_GetTimeRemainingMs(engine) + 999u) / 1000u;
    uint32_t mins = time_left_s / 60u;
    uint32_t secs = time_left_s % 60u;

    safe_rect(0, 0, FISHING_SCREEN_WIDTH, 22, COL_NAVY, 1);
    safe_rect(0, 20, FISHING_SCREEN_WIDTH, 2, COL_BLUE, 1);

    sprintf(text, "Score:%u", engine->score);
    LCD_printString(text, 6, 6, HUD_TEXT, 2);
    sprintf(text, "%02lu:%02lu", (unsigned long)mins, (unsigned long)secs);
    LCD_printString(text, 168, 6, HUD_TEXT, 2);

}

static void draw_result_fish_icon(int16_t x, int16_t y, FishType_t type)
{
    Fish_t icon;
    icon.type = type;
    icon.width = fish_width(type) - 4;
    icon.height = fish_height(type);
    icon.x = x;
    icon.y = y;
    icon.vx = 1;
    icon.active = 1;
    icon.respawn_tick = 0;
    draw_fish_body(&icon);
}

static void draw_result_option(int16_t x, int16_t y, const char* text, uint8_t selected)
{
    uint8_t fill = selected ? COL_YELLOW : BG_PANEL_ACCENT;
    uint8_t text_col = selected ? COL_NAVY : COL_WHITE;

    safe_rect(x, y, 142, 20, fill, 1);
    safe_rect(x, y, 142, 20, COL_WHITE, 0);
    if (selected) {
        LCD_printString(">", x + 8, y + 5, COL_NAVY, 1);
        LCD_printString(text, x + 24, y + 5, text_col, 1);
    } else {
        LCD_printString(text, x + 24, y + 5, text_col, 1);
    }
}

static void draw_results(FishingEngine_t* engine)
{
    char text[32];

    draw_play_background();
    draw_panel(22, 18, 196, 204);

    LCD_printString("Time Up", 76, 30, RESULT_HIGHLIGHT, 3);

    draw_result_fish_icon(38, 66, FISH_TYPE_SMALL);
    sprintf(text, "Small x %u", engine->caught_small);
    LCD_printString(text, 76, 74, COL_WHITE, 1);

    draw_result_fish_icon(38, 94, FISH_TYPE_MEDIUM);
    sprintf(text, "Medium x %u", engine->caught_medium);
    LCD_printString(text, 76, 102, COL_WHITE, 1);

    draw_result_fish_icon(38, 124, FISH_TYPE_BIG);
    sprintf(text, "Big x %u", engine->caught_big);
    LCD_printString(text, 76, 132, COL_WHITE, 1);

    sprintf(text, "Total Value: %u", engine->score);
    LCD_printString(text, 58, 156, RESULT_HIGHLIGHT, 1);

    draw_result_option(49, 174, "Return Main", engine->selected_menu_index == 0u);
    draw_result_option(49, 198, "Go Cooking", engine->selected_menu_index == 1u);
}

static void draw_countdown(FishingEngine_t* engine)
{
    char text[4];
    uint32_t elapsed = HAL_GetTick() - engine->countdown_start_tick;
    uint8_t number = 1;

    if (elapsed < COUNTDOWN_DURATION_MS) {
        number = (uint8_t)(3u - (elapsed / 1000u));
        if (number == 0u) {
            number = 1u;
        }
    }

    draw_play_background();
    for (uint8_t i = 0; i < FISHING_MAX_FISH; ++i) {
        draw_fish(&engine->fish[i]);
    }
    for (uint8_t i = 0; i < FISHING_MAX_ROCKS; ++i) {
        draw_hazard_rock(&engine->rocks[i]);
    }
    draw_diver(&engine->diver);
    draw_hud(engine);

    draw_panel(58, 68, 124, 96);
    LCD_printString("GET READY", 78, 82, COL_WHITE, 1);
    sprintf(text, "%u", number);
    LCD_printString(text, 106, 102, COL_YELLOW, 6);
    LCD_printString("Dash:BTN2", 82, 146, COL_CYAN, 1);
}

void FishingEngine_Draw(FishingEngine_t* engine)
{
    switch (engine->state) {
        case FISHING_STATE_MENU:
            draw_menu(engine);
            break;

        case FISHING_STATE_COUNTDOWN:
            draw_countdown(engine);
            break;

        case FISHING_STATE_PLAYING:
            draw_play_background();
            for (uint8_t i = 0; i < FISHING_MAX_FISH; ++i) {
                draw_fish(&engine->fish[i]);
            }
            for (uint8_t i = 0; i < FISHING_MAX_ROCKS; ++i) {
                draw_hazard_rock(&engine->rocks[i]);
            }
            draw_dash_trail(&engine->diver);
            draw_diver(&engine->diver);
            draw_hud(engine);
            if (engine->penalty_message_tick != 0 && (int32_t)(engine->penalty_message_tick - HAL_GetTick()) > 0) {
                draw_penalty_popup();
            }
            LCD_printString("BTN3:spear BTN2:dash", 56, 226, COL_WHITE, 1);
            break;

        case FISHING_STATE_RESULTS:
            draw_results(engine);
            break;

        default:
            draw_menu(engine);
            break;
    }
}

uint8_t FishingEngine_IsGameOver(FishingEngine_t* engine)
{
    return engine->finished;
}

uint8_t FishingEngine_GetNextAction(FishingEngine_t* engine)
{
    return engine->next_action;
}

uint16_t FishingEngine_GetScore(FishingEngine_t* engine)
{
    return engine->score;
}

uint32_t FishingEngine_GetTimeRemainingMs(FishingEngine_t* engine)
{
    if (engine->state != FISHING_STATE_PLAYING) {
        return engine->game_duration_ms;
    }

    uint32_t elapsed = (HAL_GetTick() - engine->game_start_tick) + engine->time_penalty_ms;
    if (elapsed >= engine->game_duration_ms) {
        return 0;
    }
    return engine->game_duration_ms - elapsed;
}

/* =========================================================
 * Menu template entry point: Game 1
 * ========================================================= */
MenuState Game1_Run(void)
{
    FishingEngine_t fishing;
    MenuState exit_state = MENU_STATE_HOME;

    FishingEngine_Init(&fishing);

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);
        UserInput input = Joystick_GetInput(&joystick_data);

        FishingEngine_Update(&fishing,
                             input,
                             current_input.btn3_pressed,
                             current_input.btn2_pressed,
                             0);

        FishingEngine_Draw(&fishing);
        LCD_Refresh(&cfg0);

        if (FishingEngine_IsGameOver(&fishing)) {
            if (FishingEngine_GetNextAction(&fishing) == FISHING_NEXT_COOKING) {
                exit_state = MENU_STATE_GAME_2;
            } else {
                exit_state = MENU_STATE_HOME;
            }
            break;
        }

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < 30U) {
            HAL_Delay(30U - frame_time);
        }
    }

    buzzer_off(&buzzer_cfg);
    PWM_SetDuty(&pwm_cfg, 50);

    return exit_state;
}
