#include "Game_2.h"
#include "InputHandler.h"
#include "LCD.h"
#include "Joystick.h"
#include "Buzzer.h"
#include "adc.h"
#include "main.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

#define GAME2_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define GAME2_LCD_WIDTH 240U
#define GAME2_LCD_HEIGHT 280U
#define GAME2_LCD_FRAME_MS 33U
#define GAME2_LOOP_DELAY_MS 10U
#define GAME2_LOADING_STEPS 18U
#define GAME2_LOADING_STEP_MS 55U
#define GAME2_BUTTON_DEBOUNCE_MS 120U
#define GAME2_CHOP_ENTER_MS 240U
#define GAME2_CHOP_FLASH_MS 120U
#define GAME2_CHOP_DONE_MS 420U
#define GAME2_CHOP_ALL_DONE_MS 5000U
#define GAME2_FISH_COUNT 3U
#define GAME2_CHOP_ROUNDS_PER_FISH 2U
#define GAME2_PASS_COUNT ((GAME2_FISH_COUNT * 2U) / 3U)

#define GAME2_COOK_INTRO_DROP_MS   1600U
#define GAME2_COOK_INTRO_STIR_MS   1800U
#define GAME2_COOK_INTRO_BUBBLE_MS 2200U
#define GAME2_COOK_INTRO_MS (GAME2_COOK_INTRO_DROP_MS + GAME2_COOK_INTRO_STIR_MS + GAME2_COOK_INTRO_BUBBLE_MS)

#define GAME2_RESULT_SHOW_MS 1600U
#define GAME2_RESULT_ARM_MS 250U
#define GAME2_COOK_SUCCESS_WINDOW_MS 2000U
#define GAME2_SUMMARY_ARM_MS 800U
#define GAME2_SUMMARY_CARD_MS 3000U
#define GAME2_PREP_TIP_CYCLE_MS 2200U
#define GAME2_SUMMARY_OPTION_RETURN_MENU 0U
#define GAME2_SUMMARY_OPTION_GO_SELLING 1U

#define GAME2_POT_X 24U
#define GAME2_POT_Y 106U

#define GAME2_POT_ADC_CHANNEL ADC_CHANNEL_10
#define GAME2_POT_SAMPLING_TIME ADC_SAMPLETIME_640CYCLES_5
#define GAME2_POT_MAX_RAW 4095U
#define GAME2_FIRE_LOW_MAX_RAW ((GAME2_POT_MAX_RAW * 33U) / 100U)
#define GAME2_FIRE_MED_MAX_RAW ((GAME2_POT_MAX_RAW * 66U) / 100U)
#define GAME2_FIRE_HYSTERESIS_RAW 180U
#define GAME2_FIRE_MISMATCH_GRACE_MS 180U
#define GAME2_POT_SAMPLE_COUNT 4U

#define GAME2_FIRE_METER_X 8U
#define GAME2_FIRE_METER_Y 198U

#define G2C_BLACK 0U
#define G2C_WHITE 1U
#define G2C_RED 2U
#define G2C_GREEN 3U
#define G2C_BLUE 4U
#define G2C_ORANGE 5U
#define G2C_YELLOW 6U
#define G2C_PINK 7U
#define G2C_PURPLE 8U
#define G2C_NAVY 9U
#define G2C_GOLD 10U
#define G2C_VIOLET 11U
#define G2C_BROWN 12U
#define G2C_GREY 13U
#define G2C_CYAN 14U
#define G2C_MAGENTA 15U

#define G2C_BG G2C_NAVY
#define G2C_TEXT G2C_WHITE
#define G2C_SUBTEXT G2C_CYAN
#define G2C_BOARD G2C_GOLD
#define G2C_BOARD_EDGE G2C_BROWN
#define G2C_POT_BODY G2C_GREY
#define G2C_POT_RIM G2C_WHITE
#define G2C_POT_DARK G2C_BLACK
#define G2C_SOUP G2C_CYAN
#define G2C_SUCCESS G2C_GREEN
#define G2C_FAIL G2C_RED
#define G2C_LOW_FIRE G2C_BLUE
#define G2C_MED_FIRE G2C_ORANGE
#define G2C_HIGH_FIRE G2C_RED
#define G2C_BIG_FISH G2C_GREY
#define G2C_BIG_FISH_HL G2C_WHITE
#define G2C_MID_FISH G2C_PURPLE
#define G2C_MID_FISH_HL G2C_VIOLET
#define G2C_SMALL_FISH G2C_GREEN
#define G2C_SMALL_FISH_HL G2C_YELLOW

#define GAME2_RGB_ACTIVE_LEVEL GPIO_PIN_SET
#define GAME2_RGB_R_PORT GPIOB
#define GAME2_RGB_R_PIN GPIO_PIN_3
#define GAME2_RGB_B_PORT GPIOB
#define GAME2_RGB_B_PIN GPIO_PIN_4
#define GAME2_RGB_G_PORT GPIOB
#define GAME2_RGB_G_PIN GPIO_PIN_5

typedef enum {
    GAME2_FISH_BIG = 0,
    GAME2_FISH_MEDIUM,
    GAME2_FISH_SMALL
} Game2FishType_t;

typedef enum {
    GAME2_FIRE_LOW = 0,
    GAME2_FIRE_MEDIUM,
    GAME2_FIRE_HIGH
} Game2FireLevel_t;

typedef enum {
    GAME2_FAIL_NONE = 0,
    GAME2_FAIL_WRONG_FIRE,
    GAME2_FAIL_TOO_EARLY,
    GAME2_FAIL_TOO_LATE
} Game2FailReason_t;

typedef enum {
    GAME2_STATE_IDLE = 0,
    GAME2_STATE_CHOP_ENTER,
    GAME2_STATE_CHOP_PLAY,
    GAME2_STATE_CHOP_HIT_ANIM,
    GAME2_STATE_CHOP_FISH_DONE,
    GAME2_STATE_CHOP_ALL_DONE,
    GAME2_STATE_COOK_INTRO,
    GAME2_STATE_COOK_PREP,
    GAME2_STATE_COOK_ACTIVE,
    GAME2_STATE_COOK_RESULT,
    GAME2_STATE_SUMMARY
} Game2State_t;

typedef struct {
    Game2FishType_t type;
    const char *dish_name;
    uint8_t chops_required;
    uint8_t target_seconds;
} Game2FishStep_t;

typedef struct {
    uint16_t freq_hz;
    uint8_t volume;
    uint16_t duration_ms;
} Game2SoundStep_t;

typedef struct {
    Game2State_t state;
    uint8_t initialized;
    uint8_t finished;
    uint8_t button_event;
    uint8_t render_requested;
    uint8_t soft_pwm_step;
    uint8_t active_fish_index;
    uint8_t chop_round_on_fish;
    uint8_t chop_hits_on_fish;
    uint8_t success_count;
    uint8_t batch_result[GAME2_FISH_COUNT];
    uint8_t last_batch_success;
    uint8_t rgb_r;
    uint8_t rgb_g;
    uint8_t rgb_b;
    uint16_t pot_raw;
    Game2FireLevel_t current_fire;
    Game2FailReason_t fail_reason;
    uint32_t state_enter_ms;
    uint32_t last_button_ms;
    uint32_t last_render_ms;
    uint32_t cook_start_ms;
    uint32_t last_cook_elapsed_ms;
    uint32_t fire_mismatch_since_ms;
    uint32_t buzzer_deadline_ms;
    uint8_t buzzer_active;
    uint8_t sound_busy;
    uint8_t sound_sequence_length;
    uint8_t sound_sequence_index;
    uint8_t cook_final_second_cued;
    uint8_t summary_selection;
    const Game2SoundStep_t *sound_sequence;
    Direction last_summary_direction;
    MenuState exit_state;
} Game2Context_t;

static Game2Context_t g_game2;
static uint8_t g_game2_adc_calibrated = 0U;

static const Game2FishStep_t kGame2FishSteps[] = {
    {GAME2_FISH_BIG, "Fish & Chips", 6U, 10U},
    {GAME2_FISH_MEDIUM, "Cod Burger", 4U, 7U},
    {GAME2_FISH_SMALL, "Cod Soup", 3U, 5U}
};

static const Game2SoundStep_t kGame2CookIntroMelody[] = {
    {NOTE_C5, 28U, 90U}, {0U, 0U, 35U},
    {NOTE_E5, 28U, 90U}, {0U, 0U, 35U},
    {NOTE_G5, 30U, 90U}, {0U, 0U, 35U},
    {NOTE_A5, 30U, 120U}, {0U, 0U, 45U},
    {NOTE_G5, 30U, 90U}, {0U, 0U, 35U},
    {NOTE_E5, 28U, 90U}, {0U, 0U, 35U},
    {NOTE_G5, 30U, 140U}
};

static const Game2SoundStep_t kGame2CookSuccessMelody[] = {
    {NOTE_G5, 28U, 80U}, {0U, 0U, 25U},
    {NOTE_B5, 28U, 80U}, {0U, 0U, 25U},
    {NOTE_D6, 30U, 110U}, {0U, 0U, 30U},
    {NOTE_G6, 32U, 180U}
};

static const Game2SoundStep_t kGame2LastSecondAlert[] = {
    {NOTE_A5, 26U, 70U}, {0U, 0U, 30U},
    {NOTE_A6, 28U, 120U}
};

static const Game2SoundStep_t kGame2SummaryMelody[] = {
    {NOTE_C5, 26U, 100U}, {0U, 0U, 35U},
    {NOTE_E5, 26U, 100U}, {0U, 0U, 35U},
    {NOTE_G5, 28U, 110U}, {0U, 0U, 40U},
    {NOTE_C6, 30U, 150U}, {0U, 0U, 45U},
    {NOTE_A5, 28U, 100U}, {0U, 0U, 35U},
    {NOTE_G5, 28U, 100U}, {0U, 0U, 35U},
    {NOTE_E5, 26U, 120U}, {0U, 0U, 35U},
    {NOTE_C5, 26U, 170U}
};

static void Game2_EnterState(Game2State_t next_state);
static void Game2_UpdateStateMachine(void);
static void Game2_UpdateChopPhase(void);
static void Game2_UpdateCookPhase(void);
static void Game2_UpdateSummary(void);
static void Game2_HandleChopButton(void);
static void Game2_AdvanceToNextChopFish(void);
static void Game2_FinishCookBatch(uint8_t success, Game2FailReason_t fail_reason, uint32_t elapsed_ms);
static void Game2_EvaluateCookStop(uint32_t elapsed_ms);
static void Game2_ServiceInput(void);
static void Game2_ServiceBuzzer(void);
static void Game2_ServiceRgbPwm(void);
static void Game2_ServiceOutputs(void);
static void Game2_ConfigureHardware(void);
static void Game2_ResetOutputs(void);
static void Game2_PlayTone(uint32_t freq_hz, uint8_t volume, uint32_t duration_ms);
static void Game2_PlaySequence(const Game2SoundStep_t *sequence, uint8_t length);
static void Game2_StartSoundStep(const Game2SoundStep_t *step);
static void Game2_RenderLoadingScreen(uint8_t progress, uint8_t frame);
static void Game2_RenderIfNeeded(void);
static void Game2_RenderScene(void);
static void Game2_RenderChopScene(uint8_t hit_flash);
static void Game2_RenderChopAllDoneScene(void);
static void Game2_RenderCookIntroScene(void);
static void Game2_RenderCookPrepScene(void);
static void Game2_RenderCookActiveScene(void);
static void Game2_RenderCookResultScene(void);
static void Game2_RenderSummaryScene(void);
static void Game2_DrawTopBar(const char *title);
static void Game2_DrawBottomHint(const char *line_a, const char *line_b);
static void Game2_DrawCenteredText(const char *text, uint16_t y, uint8_t colour, uint8_t size);
static void Game2_DrawTextInBox(const char *text, uint16_t x, uint16_t y, uint16_t width, uint8_t colour, uint8_t size);
static void Game2_DrawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t percent, uint8_t colour);
static void Game2_DrawPrepBackground(void);
static void Game2_DrawKitchenBackground(uint8_t warm_mode);
static void Game2_DrawCounterTop(uint16_t y, uint8_t colour_a, uint8_t colour_b);
static void Game2_DrawSteamPuffs(uint16_t x, uint16_t y, uint8_t frame);
static void Game2_DrawSparkle(uint16_t x, uint16_t y, uint8_t colour);
static void Game2_DrawChopBoard(void);
static void Game2_DrawScaledFish(Game2FishType_t type, uint16_t x, uint16_t y, uint8_t scale, uint8_t flash_white);
static uint8_t Game2_GetFishDrawWidth(Game2FishType_t type);
static uint8_t Game2_GetFishDrawHeight(Game2FishType_t type);
static uint8_t Game2_GetFishSourceWidth(Game2FishType_t type);
static uint8_t Game2_GetFishSourceHeight(Game2FishType_t type);
static void Game2_DrawMappedFishPixel(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                      uint8_t source_width, uint8_t source_height,
                                      uint8_t source_x, uint8_t source_y, uint8_t colour);
static void Game2_DrawMappedFishHLine(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                      uint8_t source_width, uint8_t source_height,
                                      int16_t x0, int16_t x1, int16_t source_y, uint8_t colour);
static void Game2_DrawMappedFishRect(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                     uint8_t source_width, uint8_t source_height,
                                     int16_t source_x, int16_t source_y, int16_t width, int16_t height, uint8_t colour);
static void Game2_DrawLargePot(uint16_t x, uint16_t y, Game2FishType_t type, uint8_t bubble_frame, uint8_t show_fish, uint8_t burnt);
static void Game2_DrawDishPlate(Game2FishType_t type, uint16_t x, uint16_t y, uint8_t scale);
static void Game2_DrawFireMeterAt(uint16_t x, uint16_t y, Game2FireLevel_t current_fire);
static uint16_t Game2_EaseOutQuad(uint32_t elapsed, uint32_t duration, uint16_t start, uint16_t end);
static void Game2_DrawFlameIcon(uint16_t x, uint16_t y, uint8_t scale, uint8_t outer_colour, uint8_t inner_colour);
static const Game2FishStep_t *Game2_GetCurrentStep(void);
static uint8_t Game2_GetFishBodyColour(Game2FishType_t type);
static uint8_t Game2_GetFishHighlightColour(Game2FishType_t type);
static Game2FireLevel_t Game2_GetTargetFireForFish(Game2FishType_t type);
static const char *Game2_GetFireName(Game2FireLevel_t fire_level);
static uint8_t Game2_GetFireColour(Game2FireLevel_t fire_level);
static uint8_t Game2_GetCurrentChopProgress(void);
static const char *Game2_GetLoadingStatus(uint8_t progress);
static uint16_t Game2_ReadPotRaw(void);
static void Game2_SampleHeat(void);
static Game2FireLevel_t Game2_ReadFireLevelFromRaw(uint16_t raw_value);
static void Game2_FormatSignedTime(char *buffer, size_t buffer_size, int32_t time_ms);
static void Game2_FormatElapsedTime(char *buffer, size_t buffer_size, uint32_t time_ms);
static void Game2_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t on, GPIO_PinState active_level);


void Game2_Reset(void)
{
    memset(&g_game2, 0, sizeof(g_game2));
    g_game2.state = GAME2_STATE_IDLE;
    g_game2.current_fire = GAME2_FIRE_LOW;
    g_game2.summary_selection = GAME2_SUMMARY_OPTION_RETURN_MENU;
    g_game2.last_summary_direction = CENTRE;
    g_game2.exit_state = MENU_STATE_HOME;
    for (uint32_t i = 0U; i < GAME2_ARRAY_LEN(g_game2.batch_result); ++i)
        g_game2.batch_result[i] = 0xFFU;
}

void Game2_Init(void)
{
    Game2_Reset();
    Game2_ConfigureHardware();
    if (g_game2_adc_calibrated == 0U) {
        if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK)
            g_game2_adc_calibrated = 1U;
    }
    LCD_Set_Palette(PALETTE_DEFAULT);
    Game2_EnterState(GAME2_STATE_CHOP_ENTER);
    g_game2.initialized = 1U;
}

void Game2_Update(void)
{
    if (!g_game2.initialized) Game2_Init();
    if (g_game2.finished) {
        Game2_ServiceBuzzer();
        Game2_ServiceOutputs();
        return;
    }
    Game2_ServiceInput();
    Game2_ServiceBuzzer();
    Game2_UpdateStateMachine();
    Game2_ServiceOutputs();
    Game2_RenderIfNeeded();
}

uint8_t Game2_IsFinished(void) { return g_game2.finished; }

void Game2_ShowLoadingScreen(void)
{
    for (uint8_t step = 0U; step <= GAME2_LOADING_STEPS; ++step) {
        uint8_t progress = (uint8_t)((step * 100U) / GAME2_LOADING_STEPS);
        Game2_RenderLoadingScreen(progress, step);
        HAL_Delay(GAME2_LOADING_STEP_MS);
    }

    current_input.btn2_pressed = 0U;
    current_input.btn3_pressed = 0U;
    g_game2.render_requested = 1U;
}

MenuState Game2_Run(void)
{
    MenuState exit_state;

    Game2_Init();
    Game2_ShowLoadingScreen();
    while (!Game2_IsFinished()) {
        Game2_Update();
        HAL_Delay(GAME2_LOOP_DELAY_MS);
    }
    exit_state = g_game2.exit_state;
    Game2_Reset();
    Game2_ResetOutputs();
    LCD_Set_Palette(PALETTE_DEFAULT);
    return exit_state;
}


static void Game2_EnterState(Game2State_t next_state)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();

    g_game2.state = next_state;
    g_game2.state_enter_ms = HAL_GetTick();
    g_game2.render_requested = 1U;
    switch (next_state) {
        case GAME2_STATE_CHOP_HIT_ANIM: Game2_PlayTone(1450U,28U,55U); break;
        case GAME2_STATE_CHOP_FISH_DONE: Game2_PlayTone(920U,30U,90U); break;
        case GAME2_STATE_CHOP_ALL_DONE: Game2_PlayTone(1180U,34U,130U); break;
        case GAME2_STATE_COOK_INTRO:
            Game2_PlaySequence(kGame2CookIntroMelody, (uint8_t)GAME2_ARRAY_LEN(kGame2CookIntroMelody));
            break;
        case GAME2_STATE_COOK_PREP:
            g_game2.fail_reason = GAME2_FAIL_NONE;
            g_game2.last_batch_success = 0U;
            g_game2.fire_mismatch_since_ms = 0U;
            Game2_SampleHeat();
            if (step && g_game2.current_fire == Game2_GetTargetFireForFish(step->type))
                Game2_PlayTone((uint32_t)NOTE_E6, 26U, 60U);
            break;
        case GAME2_STATE_COOK_ACTIVE:
            g_game2.cook_start_ms = g_game2.state_enter_ms;
            g_game2.last_cook_elapsed_ms = 0U;
            g_game2.cook_final_second_cued = 0U;
            g_game2.fire_mismatch_since_ms = 0U;
            Game2_SampleHeat();
            Game2_PlayTone(980U,22U,45U);
            break;
        case GAME2_STATE_COOK_RESULT:
            g_game2.fire_mismatch_since_ms = 0U;
            if (g_game2.last_batch_success) {
                Game2_PlaySequence(kGame2CookSuccessMelody, (uint8_t)GAME2_ARRAY_LEN(kGame2CookSuccessMelody));
            } else {
                Game2_PlayTone(220U,45U,650U);
            }
            break;
        case GAME2_STATE_SUMMARY:
            g_game2.summary_selection = GAME2_SUMMARY_OPTION_RETURN_MENU;
            g_game2.last_summary_direction = CENTRE;
            Game2_PlaySequence(kGame2SummaryMelody, (uint8_t)GAME2_ARRAY_LEN(kGame2SummaryMelody));
            break;
        default:
            break;
    }
}

static void Game2_UpdateStateMachine(void)
{
    switch (g_game2.state) {
        case GAME2_STATE_CHOP_ENTER: case GAME2_STATE_CHOP_PLAY:
        case GAME2_STATE_CHOP_HIT_ANIM: case GAME2_STATE_CHOP_FISH_DONE:
        case GAME2_STATE_CHOP_ALL_DONE: Game2_UpdateChopPhase(); break;
        case GAME2_STATE_COOK_INTRO: case GAME2_STATE_COOK_PREP:
        case GAME2_STATE_COOK_ACTIVE: case GAME2_STATE_COOK_RESULT:
            Game2_UpdateCookPhase(); break;
        case GAME2_STATE_SUMMARY: Game2_UpdateSummary(); break;
        default: break;
    }
}

static void Game2_UpdateChopPhase(void)
{
    uint32_t elapsed = HAL_GetTick() - g_game2.state_enter_ms;
    switch (g_game2.state) {
        case GAME2_STATE_CHOP_ENTER:
            if (elapsed >= GAME2_CHOP_ENTER_MS) {
                Game2_EnterState(GAME2_STATE_CHOP_PLAY);
            }
            break;
        case GAME2_STATE_CHOP_PLAY:
            if (g_game2.button_event) {
                Game2_HandleChopButton();
            }
            break;
        case GAME2_STATE_CHOP_HIT_ANIM:
            if (elapsed >= GAME2_CHOP_FLASH_MS) {
                const Game2FishStep_t *s = Game2_GetCurrentStep();
                if (s && g_game2.chop_hits_on_fish >= s->chops_required) {
                    Game2_EnterState(GAME2_STATE_CHOP_FISH_DONE);
                } else {
                    Game2_EnterState(GAME2_STATE_CHOP_PLAY);
                }
            }
            break;
        case GAME2_STATE_CHOP_FISH_DONE:
            if (elapsed >= GAME2_CHOP_DONE_MS) {
                Game2_AdvanceToNextChopFish();
            }
            break;
        case GAME2_STATE_CHOP_ALL_DONE:
            if (elapsed >= GAME2_CHOP_ALL_DONE_MS) {
                g_game2.active_fish_index = 0U;
                Game2_EnterState(GAME2_STATE_COOK_INTRO);
            }
            break;
        default:
            break;
    }
}

static void Game2_UpdateCookPhase(void)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    Game2FireLevel_t target_fire;
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - g_game2.state_enter_ms;

    if (step == NULL && g_game2.state >= GAME2_STATE_COOK_PREP) return;

    switch (g_game2.state) {
        case GAME2_STATE_COOK_INTRO:
            if (!g_game2.sound_busy) {
                Game2_PlaySequence(kGame2CookIntroMelody, (uint8_t)GAME2_ARRAY_LEN(kGame2CookIntroMelody));
            }
            if (elapsed >= GAME2_COOK_INTRO_MS) {
                g_game2.active_fish_index = 0U;
                Game2_EnterState(GAME2_STATE_COOK_PREP);
            }
            break;
        case GAME2_STATE_COOK_PREP:
            if (step) {
                Game2FireLevel_t previous_fire = g_game2.current_fire;
                target_fire = Game2_GetTargetFireForFish(step->type);
                Game2_SampleHeat();
                if (g_game2.current_fire != previous_fire && g_game2.current_fire == target_fire)
                    Game2_PlayTone((uint32_t)NOTE_E6, 26U, 60U);
                if (g_game2.button_event && elapsed >= 150U) {
                    if (g_game2.current_fire == target_fire) {
                        Game2_EnterState(GAME2_STATE_COOK_ACTIVE);
                    } else {
                        Game2_FinishCookBatch(0U, GAME2_FAIL_WRONG_FIRE, 0U);
                        Game2_EnterState(GAME2_STATE_COOK_RESULT);
                    }
                }
            }
            break;
        case GAME2_STATE_COOK_ACTIVE: {
            if (!step) return;
            target_fire = Game2_GetTargetFireForFish(step->type);
            uint32_t target_ms = (uint32_t)step->target_seconds * 1000U;
            uint32_t cook_ms = now - g_game2.cook_start_ms;
            Game2_SampleHeat();
            if (!g_game2.cook_final_second_cued && cook_ms >= (target_ms - 1000U) && cook_ms < target_ms) {
                g_game2.cook_final_second_cued = 1U;
                Game2_PlaySequence(kGame2LastSecondAlert, (uint8_t)GAME2_ARRAY_LEN(kGame2LastSecondAlert));
            }
            if (g_game2.current_fire != target_fire) {
                if (g_game2.fire_mismatch_since_ms == 0U) {
                    g_game2.fire_mismatch_since_ms = now;
                } else if ((now - g_game2.fire_mismatch_since_ms) >= GAME2_FIRE_MISMATCH_GRACE_MS) {
                    Game2_FinishCookBatch(0U, GAME2_FAIL_WRONG_FIRE, cook_ms);
                    Game2_EnterState(GAME2_STATE_COOK_RESULT);
                    return;
                }
            } else {
                g_game2.fire_mismatch_since_ms = 0U;
            }
            if (g_game2.button_event) {
                if (g_game2.current_fire != target_fire) {
                    Game2_FinishCookBatch(0U, GAME2_FAIL_WRONG_FIRE, cook_ms);
                    Game2_EnterState(GAME2_STATE_COOK_RESULT);
                    return;
                }
                Game2_EvaluateCookStop(cook_ms); return;
            }
            if (cook_ms > ((uint32_t)step->target_seconds*1000U + GAME2_COOK_SUCCESS_WINDOW_MS)) {
                Game2_FinishCookBatch(0U, GAME2_FAIL_TOO_LATE, cook_ms);
                Game2_EnterState(GAME2_STATE_COOK_RESULT);
            }
        } break;
        case GAME2_STATE_COOK_RESULT:
            if (elapsed >= GAME2_RESULT_SHOW_MS ||
                (elapsed >= GAME2_RESULT_ARM_MS && g_game2.button_event)) {
                if ((g_game2.active_fish_index+1U) < GAME2_ARRAY_LEN(kGame2FishSteps)) {
                    g_game2.active_fish_index++;
                    Game2_EnterState(GAME2_STATE_COOK_PREP);
                } else Game2_EnterState(GAME2_STATE_SUMMARY);
            } break;
        default: break;
    }
}

static void Game2_UpdateSummary(void)
{
    if (!g_game2.sound_busy) {
        Game2_PlaySequence(kGame2SummaryMelody, (uint8_t)GAME2_ARRAY_LEN(kGame2SummaryMelody));
    }
    if ((HAL_GetTick() - g_game2.state_enter_ms) >= GAME2_SUMMARY_ARM_MS && g_game2.button_event) {
        g_game2.exit_state = (g_game2.summary_selection == GAME2_SUMMARY_OPTION_GO_SELLING) ?
                             MENU_STATE_GAME_3 : MENU_STATE_HOME;
        g_game2.finished = 1U; Game2_ResetOutputs();
    }
}

static void Game2_HandleChopButton(void)
{
    const Game2FishStep_t *s = Game2_GetCurrentStep();
    if (!s) return;
    if (g_game2.chop_hits_on_fish < s->chops_required) {
        g_game2.chop_hits_on_fish++;
        Game2_EnterState(GAME2_STATE_CHOP_HIT_ANIM);
    }
}

static void Game2_AdvanceToNextChopFish(void)
{
    g_game2.chop_hits_on_fish = 0U;
    if ((g_game2.chop_round_on_fish + 1U) < GAME2_CHOP_ROUNDS_PER_FISH) {
        g_game2.chop_round_on_fish++;
        Game2_EnterState(GAME2_STATE_CHOP_ENTER);
    } else if ((g_game2.active_fish_index+1U) < GAME2_ARRAY_LEN(kGame2FishSteps)) {
        g_game2.chop_round_on_fish = 0U;
        g_game2.active_fish_index++;
        Game2_EnterState(GAME2_STATE_CHOP_ENTER);
    } else {
        g_game2.active_fish_index = 0U;
        g_game2.chop_round_on_fish = 0U;
        Game2_EnterState(GAME2_STATE_CHOP_ALL_DONE);
    }
}

static void Game2_FinishCookBatch(uint8_t suc, Game2FailReason_t reason, uint32_t ms)
{
    g_game2.last_batch_success = suc; g_game2.fail_reason = reason;
    g_game2.last_cook_elapsed_ms = ms;
    g_game2.batch_result[g_game2.active_fish_index] = suc ? 1U : 0U;
    if (suc) g_game2.success_count++;
}

static void Game2_EvaluateCookStop(uint32_t ms)
{
    const Game2FishStep_t *s = Game2_GetCurrentStep();
    if (!s) return;
    int32_t err = (int32_t)ms - (int32_t)s->target_seconds*1000;
    if (err < -(int32_t)GAME2_COOK_SUCCESS_WINDOW_MS)
        Game2_FinishCookBatch(0U, GAME2_FAIL_TOO_EARLY, ms);
    else if (err > (int32_t)GAME2_COOK_SUCCESS_WINDOW_MS)
        Game2_FinishCookBatch(0U, GAME2_FAIL_TOO_LATE, ms);
    else Game2_FinishCookBatch(1U, GAME2_FAIL_NONE, ms);
    Game2_EnterState(GAME2_STATE_COOK_RESULT);
}


static void Game2_ServiceInput(void)
{
    Direction current_direction;
    uint8_t move_left;
    uint8_t move_right;
    uint32_t now = HAL_GetTick();
    g_game2.button_event = 0U;
    Input_Read();
    Joystick_Read(&joystick_cfg, &joystick_data);
    if (current_input.btn3_pressed && (now - g_game2.last_button_ms) >= GAME2_BUTTON_DEBOUNCE_MS) {
        g_game2.last_button_ms = now;
        g_game2.button_event = 1U;
        g_game2.render_requested = 1U;
    }

    current_direction = joystick_data.direction;
    if (g_game2.state != GAME2_STATE_SUMMARY) {
        g_game2.last_summary_direction = CENTRE;
        return;
    }

    move_left = (current_direction == W || current_direction == NW || current_direction == SW);
    move_right = (current_direction == E || current_direction == NE || current_direction == SE);

    if (move_left &&
        g_game2.last_summary_direction != W &&
        g_game2.last_summary_direction != NW &&
        g_game2.last_summary_direction != SW &&
        g_game2.summary_selection != GAME2_SUMMARY_OPTION_RETURN_MENU) {
        g_game2.summary_selection = GAME2_SUMMARY_OPTION_RETURN_MENU;
        g_game2.render_requested = 1U;
    } else if (move_right &&
               g_game2.last_summary_direction != E &&
               g_game2.last_summary_direction != NE &&
               g_game2.last_summary_direction != SE &&
               g_game2.summary_selection != GAME2_SUMMARY_OPTION_GO_SELLING) {
        g_game2.summary_selection = GAME2_SUMMARY_OPTION_GO_SELLING;
        g_game2.render_requested = 1U;
    }

    g_game2.last_summary_direction = current_direction;
}

static void Game2_ServiceBuzzer(void)
{
    if (!g_game2.sound_busy || HAL_GetTick() < g_game2.buzzer_deadline_ms) return;

    if (g_game2.sound_sequence != NULL &&
        (g_game2.sound_sequence_index + 1U) < g_game2.sound_sequence_length) {
        g_game2.sound_sequence_index++;
        Game2_StartSoundStep(&g_game2.sound_sequence[g_game2.sound_sequence_index]);
        return;
    }

    buzzer_off(&buzzer_cfg);
    g_game2.buzzer_active = 0U;
    g_game2.sound_busy = 0U;
    g_game2.sound_sequence = NULL;
    g_game2.sound_sequence_length = 0U;
    g_game2.sound_sequence_index = 0U;
}

static void Game2_ServiceOutputs(void)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    uint32_t now = HAL_GetTick();
    uint8_t led = 0U;
    uint8_t rgb_blink_slow = (uint8_t)(((now / 250U) % 2U) == 0U);

    switch (g_game2.state) {
        case GAME2_STATE_CHOP_ENTER: case GAME2_STATE_CHOP_PLAY:
        case GAME2_STATE_CHOP_HIT_ANIM: case GAME2_STATE_CHOP_FISH_DONE:
            led = ((now / 220U) % 2U) == 0U; break;
        case GAME2_STATE_CHOP_ALL_DONE:
            led = ((now / 250U) % 2U) == 0U; break;
        case GAME2_STATE_COOK_INTRO:
            led = ((now / 200U) % 2U) == 0U; break;
        case GAME2_STATE_COOK_ACTIVE:
            led = ((now / 120U) % 2U) == 0U; break;
        case GAME2_STATE_COOK_RESULT:
            led = ((now / 250U) % 2U) == 0U; break;
        case GAME2_STATE_SUMMARY:
            led = ((now / 260U) % 2U) == 0U; break;
        default: break;
    }
    HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, led ? GPIO_PIN_SET : GPIO_PIN_RESET);

    g_game2.rgb_r = g_game2.rgb_g = g_game2.rgb_b = 0U;

    /* RGB LED rule table:
       Chop big fish    : yellow, constant during prep
       Chop medium fish : purple, constant during prep
       Chop small fish  : green, constant during prep
       Chop all done    : white flashing for 5 seconds
       Cooking phase    : red only; low fire dimmest, high fire brightest
       Cook result      : white flashing
    */
    if ((g_game2.state == GAME2_STATE_CHOP_ENTER ||
         g_game2.state == GAME2_STATE_CHOP_PLAY ||
         g_game2.state == GAME2_STATE_CHOP_HIT_ANIM ||
         g_game2.state == GAME2_STATE_CHOP_FISH_DONE) && step) {
        if (step->type == GAME2_FISH_BIG) {
            g_game2.rgb_r = 100U; g_game2.rgb_g = 100U; g_game2.rgb_b = 0U;    /* yellow */
        } else if (step->type == GAME2_FISH_MEDIUM) {
            g_game2.rgb_r = 80U;  g_game2.rgb_g = 0U;   g_game2.rgb_b = 100U;  /* purple */
        } else {
            g_game2.rgb_r = 0U;   g_game2.rgb_g = 100U; g_game2.rgb_b = 0U;     /* green */
        }
    } else if (g_game2.state == GAME2_STATE_CHOP_ALL_DONE) {
        if (rgb_blink_slow) {
            g_game2.rgb_r = 100U; g_game2.rgb_g = 100U; g_game2.rgb_b = 100U;  /* white flashing */
        }
    } else if (g_game2.state == GAME2_STATE_COOK_INTRO ||
               g_game2.state == GAME2_STATE_COOK_PREP ||
               g_game2.state == GAME2_STATE_COOK_ACTIVE) {
        if (g_game2.current_fire == GAME2_FIRE_LOW) {
            g_game2.rgb_r = 30U;   /* low fire: dim red */
        } else if (g_game2.current_fire == GAME2_FIRE_MEDIUM) {
            g_game2.rgb_r = 65U;   /* medium fire: medium red */
        } else {
            g_game2.rgb_r = 100U;  /* high fire: brightest red */
        }
    } else if (g_game2.state == GAME2_STATE_COOK_RESULT) {
        if (rgb_blink_slow) {
            g_game2.rgb_r = 100U; g_game2.rgb_g = 100U; g_game2.rgb_b = 100U;  /* white flashing */
        }
    } else if (g_game2.state == GAME2_STATE_SUMMARY) {
        if (rgb_blink_slow) {
            g_game2.rgb_r = 100U; g_game2.rgb_g = 100U; g_game2.rgb_b = 100U;  /* final result reminder */
        }
    }

    Game2_ServiceRgbPwm();
}

static void Game2_ServiceRgbPwm(void)
{
    g_game2.soft_pwm_step = (g_game2.soft_pwm_step+1U) & 0x0FU;
    uint8_t rt = (uint8_t)(((uint16_t)g_game2.rgb_r*16U+99U)/100U);
    uint8_t gt = (uint8_t)(((uint16_t)g_game2.rgb_g*16U+99U)/100U);
    uint8_t bt = (uint8_t)(((uint16_t)g_game2.rgb_b*16U+99U)/100U);
    Game2_WritePin(GAME2_RGB_R_PORT, GAME2_RGB_R_PIN, g_game2.soft_pwm_step<rt, GAME2_RGB_ACTIVE_LEVEL);
    Game2_WritePin(GAME2_RGB_G_PORT, GAME2_RGB_G_PIN, g_game2.soft_pwm_step<gt, GAME2_RGB_ACTIVE_LEVEL);
    Game2_WritePin(GAME2_RGB_B_PORT, GAME2_RGB_B_PIN, g_game2.soft_pwm_step<bt, GAME2_RGB_ACTIVE_LEVEL);
}

static void Game2_ConfigureHardware(void)
{
    GPIO_InitTypeDef g={0}; __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
    g.Pin=GAME2_RGB_R_PIN|GAME2_RGB_B_PIN|GAME2_RGB_G_PIN;
    HAL_GPIO_Init(GPIOB,&g); Game2_ResetOutputs();
}

static void Game2_ResetOutputs(void)
{
    HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB,GAME2_RGB_R_PIN|GAME2_RGB_B_PIN|GAME2_RGB_G_PIN,GPIO_PIN_RESET);
    g_game2.rgb_r=g_game2.rgb_g=g_game2.rgb_b=0U;
    buzzer_off(&buzzer_cfg);
    g_game2.buzzer_active = 0U;
    g_game2.sound_busy = 0U;
    g_game2.sound_sequence = NULL;
    g_game2.sound_sequence_length = 0U;
    g_game2.sound_sequence_index = 0U;
}

static void Game2_PlayTone(uint32_t f,uint8_t v,uint32_t d)
{
    g_game2.sound_sequence = NULL;
    g_game2.sound_sequence_length = 0U;
    g_game2.sound_sequence_index = 0U;
    if (d == 0U || v == 0U || f == 0U) {
        buzzer_off(&buzzer_cfg);
        g_game2.buzzer_active = 0U;
        g_game2.sound_busy = 0U;
        return;
    }
    buzzer_tone(&buzzer_cfg, f, v);
    g_game2.buzzer_active = 1U;
    g_game2.sound_busy = 1U;
    g_game2.buzzer_deadline_ms = HAL_GetTick() + d;
}

static void Game2_PlaySequence(const Game2SoundStep_t *sequence, uint8_t length)
{
    if (sequence == NULL || length == 0U) {
        Game2_PlayTone(0U, 0U, 0U);
        return;
    }

    g_game2.sound_sequence = sequence;
    g_game2.sound_sequence_length = length;
    g_game2.sound_sequence_index = 0U;
    Game2_StartSoundStep(&sequence[0]);
}

static void Game2_StartSoundStep(const Game2SoundStep_t *step)
{
    if (step == NULL) return;

    if (step->freq_hz == 0U || step->volume == 0U) {
        buzzer_off(&buzzer_cfg);
        g_game2.buzzer_active = 0U;
    } else {
        buzzer_tone(&buzzer_cfg, step->freq_hz, step->volume);
        g_game2.buzzer_active = 1U;
    }

    g_game2.sound_busy = 1U;
    g_game2.buzzer_deadline_ms = HAL_GetTick() + step->duration_ms;
}

static void Game2_RenderLoadingScreen(uint8_t progress, uint8_t frame)
{
    char progress_text[8];
    Game2FishType_t preview_type = kGame2FishSteps[(frame / 6U) % GAME2_FISH_COUNT].type;

    Game2_DrawKitchenBackground(1U);
    Game2_DrawTopBar("GAME 2");
    Game2_DrawCenteredText("FISH KITCHEN", 40U, G2C_GOLD, 2U);
    Game2_DrawCenteredText("Preparing the next order", 64U, G2C_SUBTEXT, 1U);
    Game2_DrawLargePot(GAME2_POT_X, 96U, preview_type, frame % 4U, 1U, 0U);

    snprintf(progress_text, sizeof(progress_text), "%3u%%", (unsigned int)progress);
    Game2_DrawCenteredText(progress_text, 194U, G2C_TEXT, 1U);
    Game2_DrawProgressBar(26U, 214U, 188U, 16U, progress, G2C_YELLOW);
    Game2_DrawBottomHint(Game2_GetLoadingStatus(progress), "Kitchen prep in progress");

    LCD_Refresh(&cfg0);
}


static void Game2_RenderIfNeeded(void)
{
    uint32_t now=HAL_GetTick();
    if (!g_game2.render_requested && (now-g_game2.last_render_ms) < GAME2_LCD_FRAME_MS) return;
    Game2_RenderScene(); g_game2.last_render_ms=now; g_game2.render_requested=0U;
}

static void Game2_RenderScene(void)
{
    switch (g_game2.state) {
        case GAME2_STATE_CHOP_ENTER: case GAME2_STATE_CHOP_PLAY:
        case GAME2_STATE_CHOP_HIT_ANIM: case GAME2_STATE_CHOP_FISH_DONE:
            Game2_RenderChopScene(g_game2.state==GAME2_STATE_CHOP_HIT_ANIM); break;
        case GAME2_STATE_CHOP_ALL_DONE: Game2_RenderChopAllDoneScene(); break;
        case GAME2_STATE_COOK_INTRO: Game2_RenderCookIntroScene(); break;
        case GAME2_STATE_COOK_PREP: Game2_RenderCookPrepScene(); break;
        case GAME2_STATE_COOK_ACTIVE: Game2_RenderCookActiveScene(); break;
        case GAME2_STATE_COOK_RESULT: Game2_RenderCookResultScene(); break;
        case GAME2_STATE_SUMMARY: Game2_RenderSummaryScene(); break;
        default: break;
    }
    LCD_Refresh(&cfg0);
}

static void Game2_RenderChopScene(uint8_t hit_flash)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    char prep_round[20];
    uint16_t fx=54,fy=122; uint8_t fs=10;
    Game2_DrawPrepBackground(); Game2_DrawTopBar("PREP STAGE");
    if (!step) return;
    snprintf(prep_round, sizeof(prep_round), "Prep %u/%u",
             (unsigned int)(g_game2.chop_round_on_fish + 1U),
             (unsigned int)GAME2_CHOP_ROUNDS_PER_FISH);
    if (step->type==GAME2_FISH_BIG) { fx=34; fy=120; fs=11; }
    else if (step->type==GAME2_FISH_MEDIUM) { fx=46; fy=126; fs=11; }
    else { fx=54; fy=132; fs=13; }
    Game2_DrawCenteredText(step->dish_name,40,G2C_TEXT,2);
    Game2_DrawProgressBar(24,72,192,12,Game2_GetCurrentChopProgress(),Game2_GetFishBodyColour(step->type));
    Game2_DrawChopBoard();
    Game2_DrawScaledFish(step->type,fx,fy,fs,hit_flash);
    if (g_game2.state==GAME2_STATE_CHOP_HIT_ANIM) {
        LCD_Draw_Line(72,116,186,182,G2C_WHITE); LCD_Draw_Line(76,112,190,178,G2C_CYAN);
        Game2_DrawSparkle(88,132,G2C_YELLOW); Game2_DrawCenteredText("CHOP!",88,G2C_YELLOW,2);
    } else if (g_game2.state==GAME2_STATE_CHOP_FISH_DONE) Game2_DrawCenteredText("NICE CUT!",88,G2C_SUCCESS,2);
    else Game2_DrawCenteredText("CHOP",88,G2C_WHITE,2);
    Game2_DrawBottomHint(prep_round,"Each fish needs two prep rounds");
}

static void Game2_RenderChopAllDoneScene(void)
{
    Game2_DrawPrepBackground(); Game2_DrawTopBar("TODAY'S MENU");
    Game2_DrawCenteredText("Kitchen Opens!",38,G2C_TEXT,2);
    Game2_DrawCenteredText("Each fish is prepped twice",60,G2C_SUBTEXT,1);
    LCD_Draw_Rect(12,90,64,110,G2C_BLACK,1); LCD_Draw_Rect(12,90,64,110,G2C_WHITE,0);
    LCD_Draw_Rect(88,90,64,110,G2C_BLACK,1); LCD_Draw_Rect(88,90,64,110,G2C_WHITE,0);
    LCD_Draw_Rect(164,90,64,110,G2C_BLACK,1); LCD_Draw_Rect(164,90,64,110,G2C_WHITE,0);
    Game2_DrawDishPlate(GAME2_FISH_BIG,18,116,1); Game2_DrawDishPlate(GAME2_FISH_MEDIUM,94,116,1);
    Game2_DrawDishPlate(GAME2_FISH_SMALL,170,116,1);
    LCD_printString("x1 Chips",18,176,G2C_TEXT,1); LCD_printString("x1 Burger",90,176,G2C_TEXT,1);
    LCD_printString("x1 Soup",170,176,G2C_TEXT,1);
    Game2_DrawSparkle(32,106,G2C_YELLOW); Game2_DrawSparkle(124,102,G2C_YELLOW); Game2_DrawSparkle(198,108,G2C_YELLOW);
    Game2_DrawBottomHint("Three dishes queued","Cook left to right");
}


static void Game2_RenderCookIntroScene(void)
{
    uint32_t elapsed = HAL_GetTick() - g_game2.state_enter_ms;
    uint32_t now = HAL_GetTick();
    uint8_t frame = (uint8_t)((now / 70U) % 8U);

    Game2_DrawKitchenBackground(1U);
    Game2_DrawTopBar("COOKING SHOW");

    if (elapsed < GAME2_COOK_INTRO_DROP_MS) {
        uint16_t big_y   = Game2_EaseOutQuad(elapsed,        GAME2_COOK_INTRO_DROP_MS, 20U, 124U);
        uint16_t mid_y   = Game2_EaseOutQuad(elapsed + 180U,  GAME2_COOK_INTRO_DROP_MS, 10U, 130U);
        uint16_t small_y = Game2_EaseOutQuad(elapsed + 320U,  GAME2_COOK_INTRO_DROP_MS, 0U,  136U);

        Game2_DrawCenteredText("Ingredients in!", 36U, G2C_TEXT, 2);
        Game2_DrawLargePot(GAME2_POT_X, GAME2_POT_Y, GAME2_FISH_BIG, frame, 0U, 0U);

        Game2_DrawScaledFish(GAME2_FISH_BIG,    42U,  big_y,   3U, 0U);
        Game2_DrawScaledFish(GAME2_FISH_MEDIUM, 104U, mid_y,   3U, 0U);
        Game2_DrawScaledFish(GAME2_FISH_SMALL,  166U, small_y, 4U, 0U);

        Game2_DrawSparkle(54U,  94U + (frame % 3U), G2C_YELLOW);
        Game2_DrawSparkle(184U, 102U + (frame % 2U), G2C_WHITE);
    }
    else if (elapsed < GAME2_COOK_INTRO_DROP_MS + GAME2_COOK_INTRO_STIR_MS) {
        uint32_t local = elapsed - GAME2_COOK_INTRO_DROP_MS;
        uint8_t stir = (uint8_t)((local / 90U) % 4U);

        Game2_DrawCenteredText("Stir the soup!", 36U, G2C_TEXT, 2);
        Game2_DrawLargePot(GAME2_POT_X, GAME2_POT_Y, GAME2_FISH_BIG, frame, 1U, 0U);

        if (stir == 0U) {
            LCD_Draw_Line(88U, 128U, 154U, 166U, G2C_YELLOW);
            LCD_Draw_Line(92U, 126U, 158U, 164U, G2C_WHITE);
        } else if (stir == 1U) {
            LCD_Draw_Line(114U, 118U, 128U, 178U, G2C_YELLOW);
            LCD_Draw_Line(118U, 118U, 132U, 178U, G2C_WHITE);
        } else if (stir == 2U) {
            LCD_Draw_Line(154U, 128U, 88U, 166U, G2C_YELLOW);
            LCD_Draw_Line(158U, 126U, 92U, 164U, G2C_WHITE);
        } else {
            LCD_Draw_Line(132U, 118U, 112U, 178U, G2C_YELLOW);
            LCD_Draw_Line(136U, 118U, 116U, 178U, G2C_WHITE);
        }

        LCD_Draw_Circle(96U + (frame * 4U), 152U, 3U, G2C_WHITE, 0);
        LCD_Draw_Circle(150U - (frame * 3U), 158U, 3U, G2C_CYAN, 0);
    }
    else {
        uint32_t local = elapsed - GAME2_COOK_INTRO_DROP_MS - GAME2_COOK_INTRO_STIR_MS;
        uint8_t pulse = (uint8_t)((local / 80U) % 6U);

        Game2_DrawCenteredText("Soup is boiling!", 36U, G2C_TEXT, 2);
        Game2_DrawLargePot(GAME2_POT_X, GAME2_POT_Y, GAME2_FISH_BIG, pulse, 1U, 0U);

        Game2_DrawSteamPuffs(58U,  106U, (uint8_t)((now / 100U) & 3U));
        Game2_DrawSteamPuffs(112U, 98U,  (uint8_t)((now / 90U)  & 3U));
        Game2_DrawSteamPuffs(168U, 106U, (uint8_t)((now / 110U) & 3U));

        LCD_Draw_Circle(72U,  154U - pulse,        4U, G2C_WHITE, 0);
        LCD_Draw_Circle(106U, 148U + (pulse % 3U), 3U, G2C_WHITE, 0);
        LCD_Draw_Circle(144U, 154U - (pulse % 4U), 4U, G2C_WHITE, 0);
    }

    Game2_DrawBottomHint("Get ready", "Set heat after animation");
}

static void Game2_RenderCookPrepScene(void)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    Game2FireLevel_t target_fire;
    uint8_t tip_index;
    char target_heat[24];
    char tip_line_1[32];
    char tip_line_2[32];

    Game2_DrawKitchenBackground(1U); Game2_DrawTopBar("HEAT CONTROL");
    if (!step) return;

    target_fire = Game2_GetTargetFireForFish(step->type);
    snprintf(target_heat, sizeof(target_heat), "Target fire: %s", Game2_GetFireName(target_fire));

    tip_index = (uint8_t)((HAL_GetTick() / GAME2_PREP_TIP_CYCLE_MS) % 4U);
    switch (tip_index) {
        case 0:
            snprintf(tip_line_1, sizeof(tip_line_1), "Big fish: %us", kGame2FishSteps[GAME2_FISH_BIG].target_seconds);
            snprintf(tip_line_2, sizeof(tip_line_2), "Use HIGH fire");
            break;
        case 1:
            snprintf(tip_line_1, sizeof(tip_line_1), "Medium fish: %us", kGame2FishSteps[GAME2_FISH_MEDIUM].target_seconds);
            snprintf(tip_line_2, sizeof(tip_line_2), "Use MED fire");
            break;
        case 2:
            snprintf(tip_line_1, sizeof(tip_line_1), "Small fish: %us", kGame2FishSteps[GAME2_FISH_SMALL].target_seconds);
            snprintf(tip_line_2, sizeof(tip_line_2), "Use LOW fire");
            break;
        default:
            snprintf(tip_line_1, sizeof(tip_line_1), "Bigger fish, stronger fire");
            snprintf(tip_line_2, sizeof(tip_line_2), "Small LOW  Mid MED  Big HIGH");
            break;
    }

    Game2_DrawCenteredText(step->dish_name,36,G2C_TEXT,2);
    Game2_DrawCenteredText(target_heat,56,Game2_GetFireColour(target_fire),1);
    LCD_Draw_Rect(16,68,208,26,G2C_BLACK,1);
    LCD_Draw_Rect(16,68,208,26,Game2_GetFireColour(target_fire),0);
    Game2_DrawCenteredText(tip_line_1,73,G2C_TEXT,1);
    Game2_DrawCenteredText(tip_line_2,83,G2C_SUBTEXT,1);

    Game2_DrawLargePot(GAME2_POT_X, GAME2_POT_Y, step->type, (HAL_GetTick()/120U)%5U, 1U, 0U);

    Game2_DrawCenteredText("Turn pot to set heat", 220, G2C_TEXT, 1);
    Game2_DrawCenteredText("BT3 confirm heat", 232, G2C_SUBTEXT, 1);
    /* LCD left-bottom heat display: LOW=1, MED=2, HIGH=3 flames. */
    Game2_DrawFireMeterAt(GAME2_FIRE_METER_X, GAME2_FIRE_METER_Y, g_game2.current_fire);
}

static void Game2_RenderCookActiveScene(void)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    if (!step) return;
    uint32_t cook_ms = HAL_GetTick()-g_game2.cook_start_ms;
    int32_t remain = (int32_t)step->target_seconds*1000 - (int32_t)cook_ms;
    char timer[12]; Game2_FormatSignedTime(timer,sizeof(timer),remain);
    uint8_t col = (remain>2000)?G2C_CYAN:(remain>=-2000)?G2C_YELLOW:G2C_FAIL;
    Game2_DrawKitchenBackground(1U); Game2_DrawTopBar("COOK NOW");
    Game2_DrawCenteredText(step->dish_name,34,G2C_TEXT,2);
    uint16_t tx = GAME2_LCD_WIDTH - (strlen(timer)*12U) - 8U;
    LCD_printString(timer,tx,30,col,2);
    Game2_DrawLargePot(GAME2_POT_X, GAME2_POT_Y, step->type, (HAL_GetTick()/80U)%6U, 1U, 0U);
    /* LCD left-bottom heat display, fixed inside visible area. */
    Game2_DrawFireMeterAt(GAME2_FIRE_METER_X, GAME2_FIRE_METER_Y, g_game2.current_fire);
    Game2_DrawCenteredText("Keep heat steady", 226, G2C_TEXT, 1);
    Game2_DrawCenteredText("BT3 stop", 236, G2C_SUBTEXT, 1);
}

static void Game2_RenderCookResultScene(void)
{
    const Game2FishStep_t *step = Game2_GetCurrentStep();
    if (!step) return;
    char actual_time[12]; Game2_FormatElapsedTime(actual_time,sizeof(actual_time),g_game2.last_cook_elapsed_ms);
    char tgt[16], act[20]; snprintf(tgt,sizeof(tgt),"Target %us",step->target_seconds);
    snprintf(act,sizeof(act),"Actual %s",actual_time);
    const char *head; const char *sub; uint8_t hcol=G2C_TEXT; uint8_t burnt=0;
    if (g_game2.last_batch_success) { head="PERFECT COOK!"; sub="Ready to serve"; hcol=G2C_SUCCESS; }
    else if (g_game2.fail_reason==GAME2_FAIL_WRONG_FIRE) { head="OVERCOOKED!"; sub="Wrong heat selected"; hcol=G2C_FAIL; burnt=1; }
    else if (g_game2.fail_reason==GAME2_FAIL_TOO_EARLY) { head="UNDERCOOKED!"; sub="Stopped too early"; hcol=G2C_FAIL; }
    else { head="OVERCOOKED!"; sub="Stayed too long"; hcol=G2C_FAIL; burnt=1; }
    Game2_DrawKitchenBackground(1U); Game2_DrawTopBar("SERVICE RESULT");
    LCD_Draw_Rect(16,34,208,194,G2C_BLACK,1); LCD_Draw_Rect(16,34,208,194,G2C_WHITE,0);
    Game2_DrawCenteredText(head,44,hcol,2); Game2_DrawCenteredText(step->dish_name,70,G2C_TEXT,2);
    Game2_DrawCenteredText(sub,92,G2C_SUBTEXT,1); Game2_DrawCenteredText(tgt,106,G2C_SUBTEXT,1);
    Game2_DrawCenteredText(act,118,G2C_SUBTEXT,1); Game2_DrawDishPlate(step->type,64,136,2);
    if (g_game2.last_batch_success) { Game2_DrawSparkle(52,142,G2C_YELLOW); Game2_DrawSparkle(188,150,G2C_YELLOW); Game2_DrawSparkle(172,196,G2C_WHITE); }
    else {
        LCD_Draw_Line(74,146,166,198,G2C_FAIL);
        LCD_Draw_Line(74,198,166,146,G2C_FAIL);
        if (burnt) {
            LCD_Draw_Circle(96,170,8,G2C_BLACK,1);
            LCD_Draw_Circle(120,180,10,G2C_BLACK,1);
            LCD_Draw_Circle(146,170,8,G2C_BLACK,1);
            Game2_DrawSteamPuffs(116,126,(HAL_GetTick()/100U)&3);
        }
    }
    Game2_DrawBottomHint("BT3 next dish or wait","Wrong heat shows burnt result");
}

static void Game2_RenderSummaryScene(void)
{
    uint32_t elapsed = HAL_GetTick()-g_game2.state_enter_ms;
    uint16_t dot_start_x = (uint16_t)((GAME2_LCD_WIDTH/2U) - (((GAME2_ARRAY_LEN(kGame2FishSteps) - 1U) * 24U) / 2U));
    uint8_t idx = (elapsed/GAME2_SUMMARY_CARD_MS)%GAME2_ARRAY_LEN(kGame2FishSteps);
    const Game2FishStep_t *step = &kGame2FishSteps[idx];
    uint8_t res = g_game2.batch_result[idx];
    char score[24];
    uint16_t left_x = 16U;
    uint16_t right_x = 128U;
    uint16_t btn_y = 206U;
    uint16_t btn_w = 96U;
    uint16_t btn_h = 26U;
    snprintf(score,sizeof(score),"%u / %u",g_game2.success_count,(unsigned int)GAME2_ARRAY_LEN(kGame2FishSteps));
    Game2_DrawKitchenBackground(0U); Game2_DrawTopBar("FINAL RESULT");
    Game2_DrawCenteredText(score,40,(g_game2.success_count>=GAME2_PASS_COUNT)?G2C_SUCCESS:G2C_YELLOW,3);
    Game2_DrawCenteredText(step->dish_name,78,G2C_TEXT,2);
    const char *rtxt; uint8_t rcol;
    if (res==1U) { rtxt="SUCCESS"; rcol=G2C_SUCCESS; }
    else if (res==0U) { rtxt="FAILED"; rcol=G2C_FAIL; }
    else { rtxt="WAIT"; rcol=G2C_YELLOW; }
    LCD_Draw_Rect(34,104,172,88,G2C_BLACK,1); LCD_Draw_Rect(34,104,172,88,G2C_WHITE,0);
    Game2_DrawDishPlate(step->type,70,118,2);
    Game2_DrawCenteredText(rtxt,170,rcol,2);
    for (uint8_t i=0;i<GAME2_ARRAY_LEN(kGame2FishSteps);i++)
        LCD_Draw_Circle(dot_start_x+i*24U,198,5,(i==idx)?G2C_YELLOW:G2C_GREY,1);

    if (g_game2.summary_selection == GAME2_SUMMARY_OPTION_RETURN_MENU) {
        LCD_Draw_Rect(left_x - 2U, btn_y - 2U, btn_w + 4U, btn_h + 4U, G2C_YELLOW, 0);
        LCD_Draw_Rect(left_x, btn_y, btn_w, btn_h, G2C_GOLD, 1);
    } else {
        LCD_Draw_Rect(left_x, btn_y, btn_w, btn_h, G2C_BLACK, 1);
    }
    LCD_Draw_Rect(left_x, btn_y, btn_w, btn_h, G2C_WHITE, 0);
    Game2_DrawTextInBox("RETURN MENU", left_x, btn_y + 8U, btn_w,
                        (g_game2.summary_selection == GAME2_SUMMARY_OPTION_RETURN_MENU) ? G2C_BLACK : G2C_TEXT, 1U);

    if (g_game2.summary_selection == GAME2_SUMMARY_OPTION_GO_SELLING) {
        LCD_Draw_Rect(right_x - 2U, btn_y - 2U, btn_w + 4U, btn_h + 4U, G2C_YELLOW, 0);
        LCD_Draw_Rect(right_x, btn_y, btn_w, btn_h, G2C_GREEN, 1);
    } else {
        LCD_Draw_Rect(right_x, btn_y, btn_w, btn_h, G2C_BLACK, 1);
    }
    LCD_Draw_Rect(right_x, btn_y, btn_w, btn_h, G2C_WHITE, 0);
    Game2_DrawTextInBox("GO SELLING", right_x, btn_y + 8U, btn_w,
                        (g_game2.summary_selection == GAME2_SUMMARY_OPTION_GO_SELLING) ? G2C_BLACK : G2C_TEXT, 1U);

    Game2_DrawBottomHint("LEFT / RIGHT TO CHOOSE", "BT3 CONFIRM");
}


static void Game2_DrawTopBar(const char *t) {
    LCD_Draw_Rect(0,0,GAME2_LCD_WIDTH,24,G2C_BLACK,1);
    LCD_Draw_Rect(0,24,GAME2_LCD_WIDTH,2,G2C_CYAN,1);
    LCD_Draw_Rect(0,0,24,24,G2C_CYAN,1); LCD_Draw_Rect(216,0,24,24,G2C_CYAN,1);
    LCD_Draw_Circle(12,12,4,G2C_WHITE,1); LCD_Draw_Circle(228,12,4,G2C_WHITE,1);
    if (t) Game2_DrawCenteredText(t,8,G2C_TEXT,1);
}

static void Game2_DrawBottomHint(const char *a,const char *b) {
    LCD_Draw_Rect(0,246,GAME2_LCD_WIDTH,34,G2C_BLACK,1);
    LCD_Draw_Rect(0,244,GAME2_LCD_WIDTH,2,G2C_CYAN,1);
    if (a) Game2_DrawCenteredText(a,252,G2C_TEXT,1);
    if (b) Game2_DrawCenteredText(b,264,G2C_SUBTEXT,1);
}

static void Game2_DrawCenteredText(const char *txt,uint16_t y,uint8_t col,uint8_t sz) {
    if (!txt) return;
    uint16_t w = (uint16_t)(strlen(txt)*6*sz);
    uint16_t x = (w<GAME2_LCD_WIDTH) ? (GAME2_LCD_WIDTH-w)/2 : 0;
    LCD_printString(txt,x,y,col,sz);
}

static void Game2_DrawTextInBox(const char *text, uint16_t x, uint16_t y, uint16_t width, uint8_t colour, uint8_t size)
{
    uint16_t text_width;
    uint16_t text_x;

    if (!text) return;

    text_width = (uint16_t)(strlen(text) * 6U * size);
    text_x = x;
    if (text_width < width) {
        text_x = (uint16_t)(x + ((width - text_width) / 2U));
    }

    LCD_printString(text, text_x, y, colour, size);
}

static void Game2_DrawProgressBar(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t p,uint8_t c) {
    LCD_Draw_Rect(x,y,w,h,G2C_BLACK,1); LCD_Draw_Rect(x,y,w,h,G2C_WHITE,0);
    uint16_t fw = (uint16_t)(((uint32_t)w*p)/100);
    if (fw>2) LCD_Draw_Rect(x+1,y+1,fw-2,h-2,c,1);
}

static void Game2_DrawPrepBackground(void) {
    LCD_Fill_Buffer(G2C_BG);
    LCD_Draw_Rect(0,26,GAME2_LCD_WIDTH,54,G2C_BLUE,1);
    LCD_Draw_Rect(0,80,GAME2_LCD_WIDTH,164,G2C_BOARD_EDGE,1);
    LCD_Draw_Rect(0,196,GAME2_LCD_WIDTH,48,G2C_BROWN,1);
    for (uint16_t y=114; y<=212; y+=22) LCD_Draw_Line(16,y,224,y,G2C_GOLD);
}

static void Game2_DrawKitchenBackground(uint8_t warm) {
    uint8_t wall = warm?G2C_NAVY:G2C_BLUE, panel = warm?G2C_GREY:G2C_BROWN;
    LCD_Fill_Buffer(wall);
    LCD_Draw_Rect(0,26,GAME2_LCD_WIDTH,50,wall,1);
    LCD_Draw_Rect(0,76,GAME2_LCD_WIDTH,16,panel,1);
    LCD_Draw_Rect(0,92,GAME2_LCD_WIDTH,152,G2C_BLACK,1);
    Game2_DrawCounterTop(208,G2C_GREY,G2C_WHITE);
}

static void Game2_DrawCounterTop(uint16_t y,uint8_t ca,uint8_t cb) {
    LCD_Draw_Rect(0,y,GAME2_LCD_WIDTH,36,ca,1); LCD_Draw_Rect(0,y,GAME2_LCD_WIDTH,4,cb,1);
    for (uint16_t x=8; x<GAME2_LCD_WIDTH; x+=18) LCD_Draw_Line(x,y+18,x+8,y+18,cb);
}

static void Game2_DrawSteamPuffs(uint16_t x,uint16_t y,uint8_t f) {
    LCD_Draw_Circle(x,y-f,6,G2C_WHITE,0); LCD_Draw_Circle(x+8,y-8-f,5,G2C_GREY,0);
    LCD_Draw_Circle(x+14,y-2-f,4,G2C_WHITE,0);
}

static void Game2_DrawSparkle(uint16_t x,uint16_t y,uint8_t c) {
    LCD_Draw_Line(x-4,y,x+4,y,c); LCD_Draw_Line(x,y-4,x,y+4,c);
    LCD_Draw_Line(x-2,y-2,x+2,y+2,c); LCD_Draw_Line(x-2,y+2,x+2,y-2,c);
}

static void Game2_DrawChopBoard(void) {
    LCD_Draw_Rect(14,108,212,126,G2C_BOARD,1); LCD_Draw_Rect(14,108,212,126,G2C_BOARD_EDGE,0);
    LCD_Draw_Rect(26,118,188,106,G2C_GOLD,0); LCD_Draw_Circle(34,126,7,G2C_BLACK,0);
    LCD_Draw_Line(58,116,198,116,G2C_GOLD); LCD_Draw_Line(52,226,190,226,G2C_GOLD);
}

static uint8_t Game2_GetFishDrawWidth(Game2FishType_t type)
{
    if (type == GAME2_FISH_BIG) return 16U;
    if (type == GAME2_FISH_MEDIUM) return 13U;
    return 10U;
}

static uint8_t Game2_GetFishDrawHeight(Game2FishType_t type)
{
    if (type == GAME2_FISH_BIG) return 9U;
    if (type == GAME2_FISH_MEDIUM) return 7U;
    return 5U;
}

static uint8_t Game2_GetFishSourceWidth(Game2FishType_t type)
{
    if (type == GAME2_FISH_BIG) return 28U;
    if (type == GAME2_FISH_MEDIUM) return 22U;
    return 18U;
}

static uint8_t Game2_GetFishSourceHeight(Game2FishType_t type)
{
    if (type == GAME2_FISH_BIG) return 16U;
    if (type == GAME2_FISH_MEDIUM) return 12U;
    return 10U;
}

static void Game2_DrawMappedFishPixel(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                      uint8_t source_width, uint8_t source_height,
                                      uint8_t source_x, uint8_t source_y, uint8_t colour)
{
    uint16_t x0 = (uint16_t)(x + (((uint32_t)source_x * target_width) / source_width));
    uint16_t x1 = (uint16_t)(x + (((uint32_t)(source_x + 1U) * target_width) / source_width));
    uint16_t y0 = (uint16_t)(y + (((uint32_t)source_y * target_height) / source_height));
    uint16_t y1 = (uint16_t)(y + (((uint32_t)(source_y + 1U) * target_height) / source_height));
    uint16_t draw_width = (x1 > x0) ? (uint16_t)(x1 - x0) : 1U;
    uint16_t draw_height = (y1 > y0) ? (uint16_t)(y1 - y0) : 1U;

    LCD_Draw_Rect(x0, y0, draw_width, draw_height, colour, 1);
}

static void Game2_DrawMappedFishHLine(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                      uint8_t source_width, uint8_t source_height,
                                      int16_t x0, int16_t x1, int16_t source_y, uint8_t colour)
{
    if (source_y < 0 || source_y >= source_height) return;
    if (x0 > x1) {
        int16_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (x1 < 0 || x0 >= source_width) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= source_width) x1 = (int16_t)source_width - 1;

    for (int16_t sx = x0; sx <= x1; ++sx) {
        Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                                  source_width, source_height,
                                  (uint8_t)sx, (uint8_t)source_y, colour);
    }
}

static void Game2_DrawMappedFishRect(uint16_t x, uint16_t y, uint16_t target_width, uint16_t target_height,
                                     uint8_t source_width, uint8_t source_height,
                                     int16_t source_x, int16_t source_y, int16_t width, int16_t height, uint8_t colour)
{
    if (width <= 0 || height <= 0) return;

    for (int16_t py = source_y; py < (source_y + height); ++py) {
        Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                                  source_width, source_height,
                                  source_x, source_x + width - 1, py, colour);
    }
}

static void Game2_DrawScaledFish(Game2FishType_t type,uint16_t x,uint16_t y,uint8_t scale,uint8_t flash) {
    uint8_t source_width = Game2_GetFishSourceWidth(type);
    uint8_t source_height = Game2_GetFishSourceHeight(type);
    uint16_t target_width = (uint16_t)Game2_GetFishDrawWidth(type) * scale;
    uint16_t target_height = (uint16_t)Game2_GetFishDrawHeight(type) * scale;
    uint8_t body = flash ? G2C_WHITE : Game2_GetFishBodyColour(type);
    uint8_t highlight = flash ? G2C_WHITE : Game2_GetFishHighlightColour(type);
    uint8_t belly = G2C_WHITE;
    uint8_t fin = flash ? G2C_WHITE : Game2_GetFishBodyColour(type);
    uint8_t outline = flash ? G2C_WHITE : G2C_BLACK;

    for (int16_t py = 0; py < source_height; ++py) {
        int16_t inset = 0;
        int16_t start;
        int16_t end;

        if (py < 2 || py >= (source_height - 2)) inset = 5;
        else if (py < 4 || py >= (source_height - 4)) inset = 3;
        else if (py == (source_height / 2)) inset = 0;
        else inset = 1;

        start = 4 + inset;
        end = (int16_t)source_width - 8 - inset;
        Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                                  source_width, source_height,
                                  start, end, py, body);
        if (py >= (source_height / 2)) {
            Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                                      source_width, source_height,
                                      start + 1, end - 1, py, belly);
        }
    }

    for (int16_t py = 2; py < (source_height - 2); ++py) {
        int16_t nose_width = (py <= (source_height / 2)) ? (py - 1) : ((source_height - 2) - py);

        if (nose_width < 0) nose_width = 0;
        Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                                  source_width, source_height,
                                  2, 3 + nose_width, py, body);
    }

    for (int16_t py = 1; py < (source_height - 1); ++py) {
        int16_t tail_width = (py <= (source_height / 2)) ? py : ((source_height - 1) - py);

        if (tail_width < 0) tail_width = 0;
        Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                                  source_width, source_height,
                                  (int16_t)source_width - 7, (int16_t)source_width - 7 + tail_width, py, fin);
    }

    Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                              source_width, source_height,
                              8, 12, 1, fin);
    Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                              source_width, source_height,
                              9, 13, 0, fin);
    Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                              source_width, source_height,
                              10, 13, source_height - 2, fin);

    if (type == GAME2_FISH_SMALL) {
        for (int16_t sx = 8; sx <= 14; sx += 3) {
            for (int16_t py = 2; py < (source_height - 2); ++py) {
                Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                                          source_width, source_height,
                                          (uint8_t)sx, (uint8_t)py, highlight);
            }
        }
    } else if (type == GAME2_FISH_MEDIUM) {
        for (int16_t sx = 7; sx <= 13; sx += 3) {
            for (int16_t py = 2; py < (source_height - 2); ++py) {
                Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                                          source_width, source_height,
                                          (uint8_t)sx, (uint8_t)py, highlight);
            }
        }
    } else {
        for (int16_t sx = 7; sx <= 14; sx += 4) {
            for (int16_t py = 3; py < (source_height - 3); ++py) {
                if (((py + sx) & 1) == 0) {
                    Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                                              source_width, source_height,
                                              (uint8_t)sx, (uint8_t)py, highlight);
                }
            }
        }
    }

    Game2_DrawMappedFishRect(x, y, target_width, target_height,
                             source_width, source_height,
                             5, 4, 2, 2, G2C_WHITE);
    Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                              source_width, source_height,
                              6, 5, G2C_BLACK);
    Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                              source_width, source_height,
                              4, 7, G2C_BLACK);
    Game2_DrawMappedFishPixel(x, y, target_width, target_height,
                              source_width, source_height,
                              5, 8, G2C_BLACK);
    Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                              source_width, source_height,
                              4, (int16_t)source_width - 9, source_height - 1, outline);
    Game2_DrawMappedFishHLine(x, y, target_width, target_height,
                              source_width, source_height,
                              5, (int16_t)source_width - 9, 0, outline);
}


static void Game2_DrawLargePot(uint16_t x,uint16_t y,Game2FishType_t t,uint8_t bub,uint8_t show,uint8_t burnt) {
    uint8_t sc = burnt?G2C_BLACK:G2C_CYAN;
    uint16_t bob = (bub%3)*2;
    LCD_Draw_Rect(x+18,y+24,156,70,G2C_POT_BODY,1);
    LCD_Draw_Rect(x+10,y+12,172,18,G2C_POT_RIM,1);
    LCD_Draw_Rect(x+8,y+10,176,22,G2C_BLACK,0);
    LCD_Draw_Rect(x+24,y+34,144,26,sc,1);
    LCD_Draw_Line(x+26,y+40,x+164,y+40,burnt?G2C_GREY:G2C_WHITE);
    LCD_Draw_Line(x+30,y+42,x+44,y+40,burnt?G2C_GREY:G2C_WHITE);
    LCD_Draw_Line(x+70,y+42,x+84,y+40,burnt?G2C_GREY:G2C_WHITE);
    LCD_Draw_Line(x+110,y+42,x+124,y+40,burnt?G2C_GREY:G2C_WHITE);
    LCD_Draw_Line(x+150,y+42,x+164,y+40,burnt?G2C_GREY:G2C_WHITE);
    LCD_Draw_Circle(x,y+48,14,G2C_BLACK,0); LCD_Draw_Circle(x+192,y+48,14,G2C_BLACK,0);
    LCD_Draw_Rect(x+36,y+94,18,2,G2C_BLACK,1); LCD_Draw_Rect(x+146,y+94,18,2,G2C_BLACK,1);
    if (show) {
        if (t==GAME2_FISH_BIG) Game2_DrawScaledFish(t,x+44,y+38+bob,2,0);
        else if (t==GAME2_FISH_MEDIUM) Game2_DrawScaledFish(t,x+54,y+40+bob,2,0);
        else Game2_DrawScaledFish(t,x+62,y+40+bob,3,0);
    }
    if (!burnt) {
        LCD_Draw_Circle(x+48,y+48+(bub*3)%8,4,G2C_WHITE,0); LCD_Draw_Circle(x+78,y+50+(bub*5)%7,3,G2C_WHITE,0);
        LCD_Draw_Circle(x+112,y+46+(bub*4)%9,4,G2C_WHITE,0); LCD_Draw_Circle(x+142,y+49+(bub*6)%8,3,G2C_WHITE,0);
        LCD_Draw_Circle(x+62,y+8,8,G2C_WHITE,0); LCD_Draw_Circle(x+96,y+2,10,G2C_GREY,0); LCD_Draw_Circle(x+128,y+8,8,G2C_WHITE,0);
    } else {
        LCD_Draw_Circle(x+68,y+4,10,G2C_GREY,0); LCD_Draw_Circle(x+102,y-4,12,G2C_GREY,0); LCD_Draw_Circle(x+136,y+6,9,G2C_GREY,0);
    }
}

static void Game2_DrawDishPlate(Game2FishType_t type,uint16_t x,uint16_t y,uint8_t scale)
{
    uint16_t px = x, py = y + 18*scale, pw = 56*scale, ph = 14*scale;
    if (type == GAME2_FISH_SMALL) {
        LCD_Draw_Rect(x+8*scale, y+16*scale, 40*scale, 14*scale, G2C_WHITE,1);
        LCD_Draw_Rect(x+8*scale, y+16*scale, 40*scale, 14*scale, G2C_BLACK,0);
        LCD_Draw_Rect(x+10*scale, y+16*scale, 36*scale, 6*scale, G2C_CYAN,1);
        LCD_Draw_Circle(x+18*scale, y+20*scale, 2*scale, G2C_WHITE,1);
        LCD_Draw_Circle(x+28*scale, y+21*scale, 2*scale, G2C_WHITE,0);
        LCD_Draw_Circle(x+38*scale, y+20*scale, 2*scale, G2C_WHITE,1);
        LCD_Draw_Line(x+18*scale, y+10*scale, x+18*scale, y+2*scale, G2C_WHITE);
        LCD_Draw_Line(x+28*scale, y+8*scale, x+28*scale, y, G2C_WHITE);
        LCD_Draw_Line(x+38*scale, y+10*scale, x+38*scale, y+2*scale, G2C_WHITE);
        return;
    }
    LCD_Draw_Rect(px,py,pw,ph,G2C_GREY,1);
    LCD_Draw_Rect(px+scale,py+scale,pw-2*scale,ph-2*scale,G2C_WHITE,1);
    LCD_Draw_Rect(px,py,pw,ph,G2C_BLACK,0);
    if (type == GAME2_FISH_BIG) {
        for (uint8_t fry=0; fry<4; fry++) {
            uint16_t fx = x+(30+fry*4)*scale, fy = y+(6+(fry%2))*scale;
            LCD_Draw_Rect(fx,fy,3*scale,12*scale,G2C_YELLOW,1);
        }
        LCD_Draw_Rect(x+8*scale, y+10*scale, 18*scale, 8*scale, G2C_GOLD,1);
        LCD_Draw_Circle(x+8*scale, y+14*scale, 4*scale, G2C_GOLD,1);
        LCD_Draw_Circle(x+26*scale, y+14*scale, 4*scale, G2C_GOLD,1);
        LCD_Draw_Line(x+10*scale, y+12*scale, x+24*scale, y+12*scale, G2C_WHITE);
    } else {
        LCD_Draw_Rect(x+10*scale, y+18*scale, 24*scale, 4*scale, G2C_GOLD,1);
        LCD_Draw_Rect(x+10*scale, y+14*scale, 24*scale, 4*scale, G2C_BROWN,1);
        LCD_Draw_Rect(x+8*scale, y+12*scale, 28*scale, 2*scale, G2C_GREEN,1);
        LCD_Draw_Rect(x+8*scale, y+8*scale, 28*scale, 6*scale, G2C_ORANGE,1);
        LCD_Draw_Circle(x+12*scale, y+11*scale, 3*scale, G2C_ORANGE,1);
        LCD_Draw_Circle(x+32*scale, y+11*scale, 3*scale, G2C_ORANGE,1);
        LCD_Draw_Circle(x+16*scale, y+10*scale, scale, G2C_WHITE,1);
        LCD_Draw_Circle(x+22*scale, y+9*scale, scale, G2C_WHITE,1);
        LCD_Draw_Circle(x+28*scale, y+10*scale, scale, G2C_WHITE,1);
    }
}

static void Game2_DrawFireMeterAt(uint16_t x, uint16_t y, Game2FireLevel_t fire)
{
    uint8_t cnt = (uint8_t)fire + 1U;
    uint8_t sc = 3U;
    uint16_t gap = 34U;

    for (uint8_t i = 0U; i < cnt; i++) {
        uint16_t cx = x + 16U + (uint16_t)i * gap;
        Game2_DrawFlameIcon(cx, y, sc, G2C_ORANGE, G2C_YELLOW);
    }
}

static uint16_t Game2_EaseOutQuad(uint32_t elapsed, uint32_t duration, uint16_t start, uint16_t end)
{
    if (duration == 0U) return end;
    if (elapsed >= duration) return end;

    uint32_t t = (elapsed * 1000U) / duration;
    uint32_t eased = 1000U - (((1000U - t) * (1000U - t)) / 1000U);
    return (uint16_t)(start + (((uint32_t)(end - start) * eased) / 1000U));
}

static void Game2_DrawFlameIcon(uint16_t x,uint16_t y,uint8_t s,uint8_t oc,uint8_t ic) {
    uint16_t my = y+4*s, by = y+7*s;
    LCD_Draw_Line(x,y,x-3*s,my,oc); LCD_Draw_Line(x,y,x+3*s,my,oc);
    LCD_Draw_Circle(x,my,3*s,oc,1);
    LCD_Draw_Line(x-2*s,by,x,my,oc); LCD_Draw_Line(x+2*s,by,x,my,oc);
    LCD_Draw_Line(x,y+s,x-s,y+3*s,ic); LCD_Draw_Line(x,y+s,x+s,y+3*s,ic);
    LCD_Draw_Circle(x,y+4*s,2*s,ic,1);
}

static const Game2FishStep_t *Game2_GetCurrentStep(void) {
    return (g_game2.active_fish_index < GAME2_ARRAY_LEN(kGame2FishSteps)) ? &kGame2FishSteps[g_game2.active_fish_index] : NULL;
}
static uint8_t Game2_GetFishBodyColour(Game2FishType_t t) {
    if (t==GAME2_FISH_BIG) return G2C_BIG_FISH;
    if (t==GAME2_FISH_MEDIUM) return G2C_MID_FISH;
    return G2C_SMALL_FISH;
}
static uint8_t Game2_GetFishHighlightColour(Game2FishType_t t) {
    if (t==GAME2_FISH_BIG) return G2C_BIG_FISH_HL;
    if (t==GAME2_FISH_MEDIUM) return G2C_MID_FISH_HL;
    return G2C_SMALL_FISH_HL;
}
static Game2FireLevel_t Game2_GetTargetFireForFish(Game2FishType_t t) {
    if (t==GAME2_FISH_BIG) return GAME2_FIRE_HIGH;
    if (t==GAME2_FISH_MEDIUM) return GAME2_FIRE_MEDIUM;
    return GAME2_FIRE_LOW;
}
static const char *Game2_GetFireName(Game2FireLevel_t f) {
    if (f==GAME2_FIRE_LOW) return "LOW";
    if (f==GAME2_FIRE_MEDIUM) return "MED";
    return "HIGH";
}
static uint8_t Game2_GetFireColour(Game2FireLevel_t f) {
    if (f==GAME2_FIRE_LOW) return G2C_LOW_FIRE;
    if (f==GAME2_FIRE_MEDIUM) return G2C_MED_FIRE;
    return G2C_HIGH_FIRE;
}
static uint8_t Game2_GetCurrentChopProgress(void) {
    const Game2FishStep_t *s = Game2_GetCurrentStep();
    if (!s || s->chops_required==0) return 0;
    return (uint8_t)((g_game2.chop_hits_on_fish*100U)/s->chops_required);
}
static const char *Game2_GetLoadingStatus(uint8_t progress) {
    if (progress < 25U) return "Washing the chopping board";
    if (progress < 55U) return "Heating the kitchen";
    if (progress < 85U) return "Preparing fresh cod";
    return "Ready to cook";
}
static uint16_t Game2_ReadPotRaw(void) {
    ADC_ChannelConfTypeDef adc={0};
    uint32_t sum = 0U;
    adc.Channel=GAME2_POT_ADC_CHANNEL; adc.Rank=ADC_REGULAR_RANK_1;
    adc.SamplingTime=GAME2_POT_SAMPLING_TIME; adc.SingleDiff=ADC_SINGLE_ENDED;
    adc.OffsetNumber = ADC_OFFSET_NONE;
    adc.Offset = 0U;
    if (HAL_ADC_ConfigChannel(&hadc1,&adc)!=HAL_OK) return 0;

    // Discard the first sample after switching to the heat potentiometer channel.
    // This makes shared ADC reads more stable when joystick and pot use ADC1 together.
    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1,10) == HAL_OK) {
            (void)HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }

    for (uint8_t i = 0U; i < GAME2_POT_SAMPLE_COUNT; ++i) {
        if (HAL_ADC_Start(&hadc1)!=HAL_OK) return (i == 0U) ? 0U : (uint16_t)(sum / i);
        if (HAL_ADC_PollForConversion(&hadc1,10)!=HAL_OK) {
            HAL_ADC_Stop(&hadc1);
            return (i == 0U) ? 0U : (uint16_t)(sum / i);
        }
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    return (uint16_t)(sum / GAME2_POT_SAMPLE_COUNT);
}
static void Game2_SampleHeat(void) {
    g_game2.pot_raw = Game2_ReadPotRaw();
    g_game2.current_fire = Game2_ReadFireLevelFromRaw(g_game2.pot_raw);
}
static Game2FireLevel_t Game2_ReadFireLevelFromRaw(uint16_t raw) {
    uint16_t low_leave_max = GAME2_FIRE_LOW_MAX_RAW + GAME2_FIRE_HYSTERESIS_RAW;
    uint16_t low_enter_max = (GAME2_FIRE_LOW_MAX_RAW > GAME2_FIRE_HYSTERESIS_RAW)
        ? (GAME2_FIRE_LOW_MAX_RAW - GAME2_FIRE_HYSTERESIS_RAW)
        : 0U;
    uint16_t med_leave_min = (GAME2_FIRE_MED_MAX_RAW > GAME2_FIRE_HYSTERESIS_RAW)
        ? (GAME2_FIRE_MED_MAX_RAW - GAME2_FIRE_HYSTERESIS_RAW)
        : 0U;
    uint16_t med_enter_min = GAME2_FIRE_MED_MAX_RAW + GAME2_FIRE_HYSTERESIS_RAW;

    if (med_enter_min > GAME2_POT_MAX_RAW) med_enter_min = GAME2_POT_MAX_RAW;

    switch (g_game2.current_fire) {
        case GAME2_FIRE_LOW:
            if (raw <= low_leave_max) return GAME2_FIRE_LOW;
            if (raw <= med_enter_min) return GAME2_FIRE_MEDIUM;
            return GAME2_FIRE_HIGH;
        case GAME2_FIRE_HIGH:
            if (raw >= med_leave_min) return GAME2_FIRE_HIGH;
            if (raw > low_enter_max) return GAME2_FIRE_MEDIUM;
            return GAME2_FIRE_LOW;
        case GAME2_FIRE_MEDIUM:
        default:
            if (raw < low_enter_max) return GAME2_FIRE_LOW;
            if (raw > med_enter_min) return GAME2_FIRE_HIGH;
            return GAME2_FIRE_MEDIUM;
    }
}
static void Game2_FormatSignedTime(char *b,size_t sz,int32_t ms) {
    if (!b||!sz) return;
    int32_t sec = (ms>=0)?(ms+500)/1000:-((-ms+500)/1000);
    snprintf(b,sz,"%lds",(long)sec);
}
static void Game2_FormatElapsedTime(char *b,size_t sz,uint32_t ms) {
    if (!b||!sz) return;
    uint32_t sec = (ms+500)/1000;
    snprintf(b,sz,"%lus",(unsigned long)sec);
}
static void Game2_WritePin(GPIO_TypeDef *p,uint16_t pin,uint8_t on,GPIO_PinState al) {
    GPIO_PinState st = on ? al : (al==GPIO_PIN_SET?GPIO_PIN_RESET:GPIO_PIN_SET);
    HAL_GPIO_WritePin(p,pin,st);
}
