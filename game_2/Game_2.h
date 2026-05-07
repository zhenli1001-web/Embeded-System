#ifndef GAME_2_H
#define GAME_2_H

#include <stdint.h>
#include "Menu.h"

void Game2_Init(void);
void Game2_Update(void);
void Game2_Reset(void);
uint8_t Game2_IsFinished(void);
void Game2_ShowLoadingScreen(void);
MenuState Game2_Run(void);

#endif // GAME_2_H
