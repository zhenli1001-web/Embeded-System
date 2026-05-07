#ifndef GAME_1_H
#define GAME_1_H

#include <stdint.h>
#include "Menu.h"
#include "Joystick.h"
#include "Utils.h"

#define FISHING_SCREEN_WIDTH   240
#define FISHING_SCREEN_HEIGHT  240
#define FISHING_MAX_FISH       5
#define FISHING_MAX_ROCKS      4

#define FISHING_NEXT_MAIN_MENU 0
#define FISHING_NEXT_COOKING   1

typedef enum {
    FISH_TYPE_SMALL = 0,
    FISH_TYPE_MEDIUM,
    FISH_TYPE_BIG
} FishType_t;

typedef enum {
    FISHING_STATE_MENU = 0,
    FISHING_STATE_COUNTDOWN,
    FISHING_STATE_PLAYING,
    FISHING_STATE_RESULTS
} FishingState_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int8_t vx;
    int8_t vy;
    FishType_t type;
    uint8_t active;
    uint32_t respawn_tick;
    uint32_t route_change_tick;
} Fish_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    uint8_t facing_right;
    uint8_t attack_active;
    uint32_t attack_end_tick;
    uint8_t dash_active;
    uint32_t dash_end_tick;
    uint32_t dash_cooldown_tick;
    int8_t dash_dx;
    int8_t dash_dy;
    int16_t trail_x[3];
    int16_t trail_y[3];
} Diver_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int8_t vx;
    int8_t vy;
    uint32_t route_change_tick;
} Rock_t;

typedef struct {
    FishingState_t state;

    Diver_t diver;
    Fish_t fish[FISHING_MAX_FISH];
    Rock_t rocks[FISHING_MAX_ROCKS];

    uint16_t score;
    uint8_t caught_small;
    uint8_t caught_medium;
    uint8_t caught_big;

    uint32_t game_start_tick;
    uint32_t countdown_start_tick;
    uint32_t game_duration_ms;
    uint32_t time_penalty_ms;
    uint8_t countdown_last_number;

    uint32_t penalty_cooldown_tick;
    uint32_t penalty_message_tick;

    uint8_t selected_menu_index;
    uint8_t menu_confirm_latched;
    uint8_t result_confirm_latched;
    uint8_t attack_latched;
    uint8_t dash_latched;

    uint8_t finished;
    uint8_t next_action;
} FishingEngine_t;

void FishingEngine_Init(FishingEngine_t* engine);
void FishingEngine_StartGame(FishingEngine_t* engine);
void FishingEngine_Update(FishingEngine_t* engine,
                          UserInput input,
                          uint8_t attack_pressed,
                          uint8_t dash_pressed,
                          uint8_t back_pressed);
void FishingEngine_Draw(FishingEngine_t* engine);
uint8_t FishingEngine_IsGameOver(FishingEngine_t* engine);
uint8_t FishingEngine_GetNextAction(FishingEngine_t* engine);
uint16_t FishingEngine_GetScore(FishingEngine_t* engine);
uint32_t FishingEngine_GetTimeRemainingMs(FishingEngine_t* engine);

MenuState Game1_Run(void);

#endif
