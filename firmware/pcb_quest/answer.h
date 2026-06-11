#pragma once

#include <Arduino.h>

// Correct 4-char code as indexes in kana_glyphs.h (GLYPH_A=0 .. GLYPH_D9=80).
//
// Placeholder for validation: ア ア ア ア
//   GLYPH_A = 0
//
// Replace before production flash.
static const uint8_t CORRECT_ANSWER[4] PROGMEM = {0, 0, 0, 0};
