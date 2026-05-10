# Fisherman Day

## Overview

**Fisherman Day** is an STM32-based embedded mini-game system.  
The project is inspired by the daily life of a fisherman and connects three gameplay stages into one complete experience:

1. Fishing
2. Cooking
3. Selling

Instead of designing three separate mini-games, this project links them together through a unified menu system and stage transition logic. Players can experience the full process from catching fish, cooking food, and finally selling the dishes to customers.

---

## Gameplay Stages

### 1. Fishing Stage

The Fishing stage is the first part of the game.  
Players control the character to catch fish within a limited time.

Main features:

- Joystick-controlled movement
- Fish catching gameplay
- Different fish types
- Obstacle avoidance
- Score system
- Timer system
- LCD display feedback
- LED and buzzer feedback

---

### 2. Cooking Stage

The Cooking stage processes the fish collected from the Fishing stage into food.

Main features:

- Fish preparation logic
- Cooking timing control
- Heat control using potentiometer
- RGB LED feedback
- LCD cooking interface
- Buzzer sound feedback
- Dish generation system

The cooked dishes are then passed to the Selling stage.

---

### 3. Selling Stage

The Selling stage is the final stage of the system and focuses on customer service gameplay.

Players need to serve the correct food to customers before their patience runs out.

Main features:

- Customer spawning system
- Customer order system
- Patience countdown system
- Dish selection system
- Gold reward system
- Wrong-order penalty logic
- LED warning effects
- Buzzer alarm feedback
- Result screen and replay logic

Available dishes include:

- Fish & Chips
- Cod Burger
- Cod Soup

---

## System Design

The project uses a modular software structure.  
Each game stage is designed as an independent module, while the main menu controls the overall game flow.

Basic flow:

```text
Main Menu
    ↓
Fishing Stage
    ↓
Cooking Stage
    ↓
Selling Stage