#pragma once

#include <Arduino.h>

// Correct 4-char code as indexes in kana_table.h (0=ア .. 45=ン).
//
// Placeholder for validation: ア ア ア ア
//   ア=0
//
// Replace before production flash.
static const uint8_t CORRECT_ANSWER[4] PROGMEM = {0, 0, 0, 0};
