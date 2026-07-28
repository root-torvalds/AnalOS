#pragma once
#include <stdint.h>
#include "../../efi.h"

extern volatile int has_keyboard_event;
extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

#include "idt.h"
#include "keyboard.h"
#include "mouse.h"
