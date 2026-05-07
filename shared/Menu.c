#include "Menu.h"
#include "LCD.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

extern ST7789V2_cfg_t cfg0;
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

static const char* menu_options[] = {
    "FISHING",
    "COOKING",
    "SELLING"
};

#define NUM_MENU_OPTIONS 3
#define MENU_FRAME_TIME_MS 30

static void Menu_DrawPixelFish(int16_t x, int16_t y, uint8_t s,
                               uint8_t base, uint8_t stripe,
                               uint8_t belly, uint8_t flip)
{
    if (!flip) {
        LCD_Draw_Rect(x + 6*s,  y + 3*s,  15*s, 8*s, base, 1);
        LCD_Draw_Rect(x + 9*s,  y + 1*s,  10*s, 3*s, base, 1);
        LCD_Draw_Rect(x + 9*s,  y + 10*s, 10*s, 3*s, belly, 1);

        LCD_Draw_Rect(x + 20*s, y + 5*s, 5*s, 4*s, base, 1);
        LCD_Draw_Rect(x + 24*s, y + 4*s, 2*s, 6*s, base, 1);

        LCD_Draw_Line(x + 6*s, y + 7*s, x, y + 2*s, base);
        LCD_Draw_Line(x + 6*s, y + 7*s, x, y + 12*s, base);
        LCD_Draw_Line(x, y + 2*s, x, y + 12*s, base);

        LCD_Draw_Rect(x + 10*s, y + 4*s, 2*s, 6*s, stripe, 1);
        LCD_Draw_Rect(x + 14*s, y + 4*s, 2*s, 6*s, stripe, 1);

        LCD_Draw_Line(x + 13*s, y + 3*s, x + 17*s, y, stripe);
        LCD_Draw_Line(x + 15*s, y + 11*s, x + 19*s, y + 14*s, stripe);

        LCD_Draw_Circle(x + 22*s, y + 6*s, 1*s, 1, 1);
    } else {
        LCD_Draw_Rect(x + 7*s,  y + 3*s,  15*s, 8*s, base, 1);
        LCD_Draw_Rect(x + 9*s,  y + 1*s,  10*s, 3*s, base, 1);
        LCD_Draw_Rect(x + 9*s,  y + 10*s, 10*s, 3*s, belly, 1);

        LCD_Draw_Rect(x + 3*s, y + 5*s, 5*s, 4*s, base, 1);
        LCD_Draw_Rect(x + 2*s, y + 4*s, 2*s, 6*s, base, 1);

        LCD_Draw_Line(x + 22*s, y + 7*s, x + 28*s, y + 2*s, base);
        LCD_Draw_Line(x + 22*s, y + 7*s, x + 28*s, y + 12*s, base);
        LCD_Draw_Line(x + 28*s, y + 2*s, x + 28*s, y + 12*s, base);

        LCD_Draw_Rect(x + 13*s, y + 4*s, 2*s, 6*s, stripe, 1);
        LCD_Draw_Rect(x + 17*s, y + 4*s, 2*s, 6*s, stripe, 1);

        LCD_Draw_Line(x + 15*s, y + 3*s, x + 11*s, y, stripe);
        LCD_Draw_Line(x + 13*s, y + 11*s, x + 9*s, y + 14*s, stripe);

        LCD_Draw_Circle(x + 6*s, y + 6*s, 1*s, 1, 1);
    }
}

static void Menu_DrawBackground(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t wave = (now / 150) % 18;

    LCD_Fill_Buffer(4);

    LCD_Draw_Rect(0, 0, 240, 240, 0, 1);
    LCD_Draw_Rect(3, 3, 234, 234, 1, 1);
    LCD_Draw_Rect(6, 6, 228, 228, 4, 1);

    LCD_Draw_Rect(6, 6, 228, 70, 9, 1);
    LCD_Draw_Rect(6, 76, 228, 52, 11, 1);
    LCD_Draw_Rect(6, 128, 228, 106, 4, 1);

    LCD_Draw_Circle(204, 30, 16, 10, 1);
    LCD_Draw_Circle(199, 25, 4, 1, 1);

    for (int x = -20; x < 260; x += 26) {
        int sx = x + wave;
        LCD_Draw_Line(sx, 87, sx + 8, 83, 1);
        LCD_Draw_Line(sx + 8, 83, sx + 16, 87, 1);
        LCD_Draw_Line(sx + 16, 87, sx + 24, 83, 1);
    }

    for (int x = -20; x < 260; x += 30) {
        int sx = x - wave;
        LCD_Draw_Line(sx, 108, sx + 10, 105, 14);
        LCD_Draw_Line(sx + 10, 105, sx + 20, 108, 14);
    }

    LCD_Draw_Rect(15, 18, 176, 34, 0, 1);
    LCD_Draw_Rect(18, 21, 170, 28, 14, 1);
    LCD_Draw_Rect(21, 24, 164, 22, 9, 1);

    LCD_printString("FISHERMAN DAY", 30, 29, 1, 2);

    Menu_DrawPixelFish(193, 51, 1, 5, 13, 1, 1);
    Menu_DrawPixelFish(20, 88, 1, 3, 10, 1, 0);
    Menu_DrawPixelFish(185, 188, 1, 13, 10, 1, 1);

    LCD_Draw_Circle(37, 94, 2, 1, 0);
    LCD_Draw_Circle(52, 202, 2, 1, 0);
    LCD_Draw_Circle(212, 216, 3, 14, 0);
}

static void Menu_DrawJoystickIcon(void)
{
    LCD_Draw_Rect(14, 177, 48, 35, 0, 1);
    LCD_Draw_Rect(17, 180, 42, 29, 3, 1);
    LCD_Draw_Rect(22, 185, 32, 19, 12, 1);

    LCD_Draw_Rect(30, 158, 12, 27, 0, 1);
    LCD_Draw_Rect(32, 158, 8, 27, 13, 1);

    LCD_Draw_Circle(36, 151, 13, 0, 1);
    LCD_Draw_Circle(36, 151, 10, 5, 1);
    LCD_Draw_Circle(32, 146, 3, 1, 1);

    LCD_Draw_Circle(24, 197, 4, 10, 1);
    LCD_Draw_Circle(49, 197, 4, 10, 1);
}

static void Menu_DrawOption(uint8_t index, uint8_t selected, const char *text, uint8_t icon)
{
    uint16_t y = 103 + index * 35;

    if (selected) {
        LCD_Draw_Rect(68, y - 2, 150, 32, 10, 1);
        LCD_Draw_Rect(71, y + 1, 144, 26, 0, 0);
        LCD_Draw_Rect(74, y + 4, 138, 20, 14, 1);

        LCD_Draw_Rect(80, y + 8, 9, 12, 5, 1);
        LCD_Draw_Line(89, y + 8, 98, y + 14, 5);
        LCD_Draw_Line(89, y + 20, 98, y + 14, 5);
    }
    else {
        LCD_Draw_Rect(72, y, 142, 28, 0, 1);
        LCD_Draw_Rect(74, y + 2, 138, 24, 9, 1);
        LCD_Draw_Rect(77, y + 5, 132, 18, 14, 0);
    }

    if (icon == 0) {
        LCD_Draw_Circle(108, y + 14, 8, 3, 1);
        LCD_Draw_Circle(112, y + 11, 2, 1, 1);
        LCD_Draw_Line(100, y + 14, 91, y + 9, 3);
        LCD_Draw_Line(100, y + 14, 91, y + 19, 3);
    }
    else if (icon == 1) {
        LCD_Draw_Rect(100, y + 12, 19, 7, 13, 1);
        LCD_Draw_Rect(103, y + 19, 13, 3, 5, 1);
        LCD_Draw_Line(119, y + 14, 129, y + 10, 1);
        LCD_Draw_Line(119, y + 15, 129, y + 19, 1);
    }
    else {
        LCD_Draw_Circle(109, y + 14, 8, 10, 1);
        LCD_Draw_Circle(109, y + 14, 5, 13, 0);
        LCD_printString("$", 105, y + 8, 1, 1);
    }

    LCD_printString((char*)text, 132, y + 7, selected ? 1 : 0, 2);
}

static void render_home_menu(MenuSystem* menu)
{
    Menu_DrawBackground();

    LCD_Draw_Rect(62, 94, 164, 119, 0, 1);
    LCD_Draw_Rect(65, 97, 158, 113, 3, 1);
    LCD_Draw_Rect(68, 100, 152, 107, 14, 0);

    Menu_DrawOption(0, menu->selected_option == 0, menu_options[0], 0);
    Menu_DrawOption(1, menu->selected_option == 1, menu_options[1], 1);
    Menu_DrawOption(2, menu->selected_option == 2, menu_options[2], 2);

    Menu_DrawJoystickIcon();

    LCD_Draw_Rect(0, 218, 240, 22, 0, 1);
    LCD_Draw_Rect(3, 221, 234, 16, 9, 1);
    LCD_printString("BTN3 SELECT", 76, 225, 1, 1);

    LCD_Refresh(&cfg0);
}

void Menu_Init(MenuSystem* menu)
{
    menu->selected_option = 0;
}

MenuState Menu_Run(MenuSystem* menu)
{
    static Direction last_direction = CENTRE;
    MenuState selected_game = MENU_STATE_HOME;

    while (1) {
        uint32_t frame_start = HAL_GetTick();

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);

        Direction current_direction = joystick_data.direction;

        if (current_direction == S && last_direction != S) {
            menu->selected_option++;
            if (menu->selected_option >= NUM_MENU_OPTIONS) {
                menu->selected_option = 0;
            }
        }
        else if (current_direction == N && last_direction != N) {
            if (menu->selected_option == 0) {
                menu->selected_option = NUM_MENU_OPTIONS - 1;
            } else {
                menu->selected_option--;
            }
        }

        last_direction = current_direction;

        if (current_input.btn3_pressed) {
            if (menu->selected_option == 0) {
                selected_game = MENU_STATE_GAME_1;
            } else if (menu->selected_option == 1) {
                selected_game = MENU_STATE_GAME_2;
            } else if (menu->selected_option == 2) {
                selected_game = MENU_STATE_GAME_3;
            }
            break;
        }

        render_home_menu(menu);

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < MENU_FRAME_TIME_MS) {
            HAL_Delay(MENU_FRAME_TIME_MS - frame_time);
        }
    }

    return selected_game;
}