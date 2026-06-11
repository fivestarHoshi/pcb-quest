// 解読せよ！電子基盤クエスト — MVP
// TinyJoypad ATtiny85 @ 16 MHz internal
#include <ssd1306xled.h>
#include <Arduino.h>

#include "answer.h"
#include "kana_glyphs.h"
#include "title_intro.h"

#define MAX_WRONG 3
#define SLOT_EMPTY 0xFF
#define CURSOR_JUDGE 4
#define IDLE_MS 16
#define DRUM_HOLD_DELAY 12
#define DRUM_REPEAT_EVERY 1
#define TITLE_BLINK_EVERY 250  // ~4 s at 16 ms/frame
#define TITLE_FIRST_BLINK 210  // first blink ~0.6 s after the title appears

#define SLOT_X0 10
#define SLOT_BOX_W 16
#define SLOT_STEP 28
#define SLOT_PAGE 3
#define JUDGE_X 40
#define JUDGE_W 48
#define JUDGE_PAGE 6

static const uint8_t ACTION_GLYPHS[][8] PROGMEM = {
  { 0x00, 0x42, 0x22, 0x1F, 0x02, 0x42, 0x7E, 0x00 }, // カ
  { 0x00, 0x10, 0x10, 0x08, 0x7C, 0x02, 0x01, 0x00 }, // イ
  { 0x00, 0x00, 0x7F, 0x08, 0x09, 0x10, 0x01, 0x00 }, // ド
  { 0x00, 0x08, 0x44, 0x43, 0x22, 0x12, 0x0E, 0x00 }, // ク
  { 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00 }, // !
};

static const uint8_t HEART_GLYPH[7] PROGMEM = {
  0x0E, 0x1F, 0x3F, 0x7E, 0x3F, 0x1F, 0x0E
};

static const uint8_t MSG_SMALL_YA[8] PROGMEM = {
  0x00, 0x00, 0x04, 0x0E, 0x34, 0x02, 0x06, 0x00
};

static const uint8_t MSG_LONG_MARK[8] PROGMEM = {
  0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00
};

static const uint8_t MSG_TOUTEN[8] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00
};

static const uint8_t MSG_DIGITS[4][6] PROGMEM = {
  { 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },
  { 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },
  { 0x42, 0x61, 0x51, 0x49, 0x46, 0x00 },
  { 0x21, 0x41, 0x45, 0x4B, 0x31, 0x00 }
};

// Note pitch = half period in us (approx; loop overhead lowers pitch slightly)
#define NOTE_C6 478
#define NOTE_E6 379
#define NOTE_G6 319
#define NOTE_A6 284
#define NOTE_B6 253
#define NOTE_C7 239
#define NOTE_ALARM_HI 417
#define NOTE_ALARM_LO 833
#define NOTE_BUZZER 2000    // quiz-style wrong buzzer (~250Hz)
#define NOTE_BUZZER_LO 2500 // ~200Hz
#define NOTE_REST 0

// Victory fanfare: rising arpeggio x2, then scale run into a held top note.
// {half period us, duration ms}
static const uint16_t CLEAR_MELODY[][2] PROGMEM = {
  { NOTE_C6, 90 }, { NOTE_E6, 90 }, { NOTE_G6, 90 }, { NOTE_C7, 200 },
  { NOTE_REST, 60 },
  { NOTE_C6, 90 }, { NOTE_E6, 90 }, { NOTE_G6, 90 }, { NOTE_C7, 200 },
  { NOTE_REST, 90 },
  { NOTE_G6, 120 }, { NOTE_A6, 120 }, { NOTE_B6, 120 }, { NOTE_C7, 600 },
};
#define CLEAR_MELODY_LEN (sizeof(CLEAR_MELODY) / sizeof(CLEAR_MELODY[0]))

static const uint8_t SPARKLE_GLYPH[8] PROGMEM = {
  0x10, 0x44, 0x38, 0xFF, 0x38, 0x44, 0x10, 0x00
};

// Sparkle spots on pages unused by the clear screen (title: 2-3, sub: 5)
static const uint8_t SPARKLE_POS[][2] PROGMEM = {
  {10, 0}, {60, 0}, {112, 0}, {24, 1}, {96, 1},
  {6, 4}, {120, 4}, {20, 6}, {104, 6}, {56, 7},
};
#define SPARKLE_COUNT (sizeof(SPARKLE_POS) / sizeof(SPARKLE_POS[0]))

static const uint8_t INVADER_GLYPH[8] PROGMEM = {
  0x78, 0x1C, 0x77, 0x3E, 0x3E, 0x77, 0x1C, 0x78
};

#define OLED_CMD_INVERT 0xA7
#define OLED_CMD_NORMAL 0xA6
#define OLED_CMD_START_LINE 0x40

static const uint8_t MSG_INPUT_HEADER[] PROGMEM = {
  GLYPH_A, GLYPH_N, GLYPH_GO, GLYPH_U, GLYPH_WO, GLYPH_I, GLYPH_RE, GLYPH_YO
};

static const uint8_t MSG_CLEAR_TITLE[] PROGMEM = {
  GLYPH_KU, GLYPH_RI, GLYPH_A
};

static const uint8_t MSG_CLEAR_SUB[] PROGMEM = {
  GLYPH_KA, GLYPH_I, GLYPH_DO, GLYPH_KU, GLYPH_SE, GLYPH_I, GLYPH_KO, GLYPH_U
};

static const uint8_t MSG_NG_TITLE[] PROGMEM = {
  GLYPH_E, GLYPH_RA
};

static const uint8_t MSG_NG_MID[] PROGMEM = {
  GLYPH_N, GLYPH_SU, GLYPH_HA
};

static const uint8_t MSG_NG_AFTER_DIGIT[] PROGMEM = {
  GLYPH_A, GLYPH_TO
};

enum State : uint8_t {
  STATE_TITLE,
  STATE_INPUT,
  STATE_ERROR,
  STATE_CLEAR,
  STATE_GAMEOVER
};

static uint8_t slots[4];
static uint8_t drum_pos[4];
static uint8_t cursor_idx;
static uint8_t last_slot_idx;
static uint8_t wrong_count;
static State state;
static bool screen_dirty;
static bool ssd1306_ready;
static bool edit_mode;

static bool prev_l;
static bool prev_r;
static bool prev_u;
static bool prev_d;
static bool prev_f;

static uint16_t adc_a0;
static uint16_t adc_a3;
static uint8_t drum_hold_dir;
static uint8_t drum_repeat_timer;
static uint8_t force_invert_slot = 0xFF;
static uint8_t title_tick;
static uint8_t pulse_tick;

static void poll_buttons() {
  adc_a0 = analogRead(A0);
  adc_a3 = analogRead(A3);
}

static bool left_pressed() {
  return adc_a0 >= 750 && adc_a0 < 950;
}

static bool right_pressed() {
  return adc_a0 > 500 && adc_a0 < 750;
}

static bool up_pressed() {
  return adc_a3 > 500 && adc_a3 < 750;
}

static bool down_pressed() {
  return adc_a3 >= 750 && adc_a3 < 950;
}

static bool fire_pressed() {
  return digitalRead(1) == LOW;
}

static void mark_dirty() {
  screen_dirty = true;
}

static uint8_t slot_glyph_id(uint8_t i) {
  return (slots[i] == SLOT_EMPTY) ? GLYPH_QUESTION : slots[i];
}

static void sound_delay_us(uint8_t delay_value) {
  while (delay_value-- != 0) {
    _delay_us(1);
  }
}

static void sound(uint8_t freq, uint8_t dur) {
  uint8_t delay_value = 255 - freq;
  for (uint8_t t = 0; t < dur; t++) {
    if (freq != 0) PORTB |= _BV(PB4);
    sound_delay_us(delay_value);
    PORTB &= ~_BV(PB4);
    sound_delay_us(delay_value);
  }
}

static void sound_start() {
  sound(70, 90);
  _delay_ms(25);
  sound(120, 70);
  _delay_ms(20);
  sound(170, 55);
  _delay_ms(25);
  sound(110, 120);
  _delay_ms(20);
  sound(190, 160);
}

// 16-bit tone engine: reaches lower octaves than sound() (which is capped
// at half period 255 us). half_us == 0 means rest.
static void tone_half_wait(uint16_t half_us) {
  while (half_us-- != 0) {
    _delay_us(1);
  }
}

static void wait_ms(uint16_t ms) {
  while (ms-- != 0) {
    _delay_ms(1);
  }
}

static void play_note(uint16_t half_us, uint16_t dur_ms) {
  if (half_us == 0) {
    wait_ms(dur_ms);
    return;
  }
  uint16_t cycles = (uint32_t)dur_ms * 500 / half_us;
  if (cycles == 0) cycles = 1;
  while (cycles-- != 0) {
    PORTB |= _BV(PB4);
    tone_half_wait(half_us);
    PORTB &= ~_BV(PB4);
    tone_half_wait(half_us);
  }
}

static uint8_t rnd_state = 0xA5;

static uint8_t rnd() {
  uint8_t r = rnd_state;
  r = (r >> 1) ^ ((r & 1) ? 0xB8 : 0);
  rnd_state = r;
  return r;
}

static void reset_input() {
  for (uint8_t i = 0; i < 4; i++) {
    slots[i] = SLOT_EMPTY;
    drum_pos[i] = 0;
  }
  cursor_idx = 0;
  last_slot_idx = 0;
  wrong_count = 0;
  drum_hold_dir = 0;
  drum_repeat_timer = 0;
  edit_mode = false;
}

static bool all_filled() {
  for (uint8_t i = 0; i < 4; i++) {
    if (slots[i] == SLOT_EMPTY) return false;
  }
  return true;
}

static bool matches_correct() {
  for (uint8_t i = 0; i < 4; i++) {
    if (slots[i] != pgm_read_byte(&CORRECT_ANSWER[i])) return false;
  }
  return true;
}

static void present_screen(void (*draw_fn)()) {
  SSD1306.ssd1306_display_off();
  draw_fn();
  SSD1306.ssd1306_display_on();
}

static void clear_screen() {
  for (uint8_t p = 0; p < 8; p++) {
    SSD1306.ssd1306_setpos(0, p);
    SSD1306.ssd1306_send_data_start();
    for (uint8_t i = 0; i < 128; i++) {
      SSD1306.ssd1306_send_byte(0x00);
    }
    SSD1306.ssd1306_send_data_stop();
  }
}

static void draw_glyph(uint8_t x, uint8_t page, uint8_t glyph_id) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < GLYPH_BYTES; c++) {
    SSD1306.ssd1306_send_byte(pgm_read_byte(&KANA_GLYPHS[glyph_id][c]));
  }
  SSD1306.ssd1306_send_data_stop();
}

static void draw_glyph_bytes(uint8_t x, uint8_t page, const uint8_t* glyph) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < GLYPH_BYTES; c++) {
    SSD1306.ssd1306_send_byte(pgm_read_byte(&glyph[c]));
  }
  SSD1306.ssd1306_send_data_stop();
}

static void draw_digit6(uint8_t x, uint8_t page, uint8_t n) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < 6; c++) {
    SSD1306.ssd1306_send_byte(pgm_read_byte(&MSG_DIGITS[n][c]));
  }
  SSD1306.ssd1306_send_data_stop();
}

static void draw_text(uint8_t x, uint8_t page, const uint8_t* ids, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    draw_glyph(x, page, pgm_read_byte(&ids[i]));
    x += 8;
  }
}

static void compose_glyph_2x(uint8_t* top, uint8_t* bottom, uint8_t x0, const uint8_t* glyph) {
  for (uint8_t c = 0; c < GLYPH_BYTES; c++) {
    uint8_t src = pgm_read_byte(glyph + c);
    uint8_t out_top = 0;
    uint8_t out_bottom = 0;
    for (uint8_t r = 0; r < 8; r++) {
      if (src & (1 << r)) {
        uint8_t y = r << 1;
        if (y < 8) {
          out_top |= (1 << y) | (1 << (y + 1));
        } else {
          y -= 8;
          out_bottom |= (1 << y) | (1 << (y + 1));
        }
      }
    }
    top[x0 + c * 2] |= out_top;
    top[x0 + c * 2 + 1] |= out_top;
    bottom[x0 + c * 2] |= out_bottom;
    bottom[x0 + c * 2 + 1] |= out_bottom;
  }
}

static void send_16px(uint8_t x, uint8_t page, const uint8_t* top, const uint8_t* bottom, uint8_t w) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < w; c++) SSD1306.ssd1306_send_byte(top[c]);
  SSD1306.ssd1306_send_data_stop();

  SSD1306.ssd1306_setpos(x, page + 1);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < w; c++) SSD1306.ssd1306_send_byte(bottom[c]);
  SSD1306.ssd1306_send_data_stop();
}

static void draw_glyph_16(uint8_t x, uint8_t page, uint8_t glyph_id) {
  uint8_t top[16] = {0};
  uint8_t bottom[16] = {0};
  compose_glyph_2x(top, bottom, 0, &KANA_GLYPHS[glyph_id][0]);
  send_16px(x, page, top, bottom, 16);
}

static void draw_glyph_bytes_16(uint8_t x, uint8_t page, const uint8_t* glyph) {
  uint8_t top[16] = {0};
  uint8_t bottom[16] = {0};
  compose_glyph_2x(top, bottom, 0, glyph);
  send_16px(x, page, top, bottom, 16);
}

static void draw_text_16(uint8_t x, uint8_t page, const uint8_t* ids, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    draw_glyph_16(x, page, pgm_read_byte(&ids[i]));
    x += 16;
  }
}

static void draw_action_label_16(uint8_t* top, uint8_t* bottom, bool invert) {
  const uint8_t label_x = 4;
  const uint8_t label_y = 4;

  for (uint8_t g = 0; g < 5; g++) {
    for (uint8_t c = 0; c < 8; c++) {
      uint8_t src = pgm_read_byte(&ACTION_GLYPHS[g][c]);
      uint8_t x = label_x + g * 8 + c;
      for (uint8_t r = 0; r < 8; r++) {
        if (!(src & (1 << r))) continue;
        uint8_t y = label_y + r;
        if (y < 8) {
          if (invert) top[x] &= ~(1 << y);
          else top[x] |= (1 << y);
        } else {
          if (invert) bottom[x] &= ~(1 << (y - 8));
          else bottom[x] |= (1 << (y - 8));
        }
      }
    }
  }
}

static void draw_action_button_16(uint8_t* top, uint8_t* bottom, bool hover) {
  if (hover) {
    for (uint8_t x = 0; x < JUDGE_W; x++) {
      top[x] = 0xFF;
      bottom[x] = 0xFF;
    }

    top[0] &= ~0x01;
    top[JUDGE_W - 1] &= ~0x01;
    bottom[0] &= ~0x80;
    bottom[JUDGE_W - 1] &= ~0x80;

    for (uint8_t x = 2; x < JUDGE_W - 2; x++) {
      top[x] &= ~0x02;
      bottom[x] &= ~0x40;
    }
    top[1] &= ~0xFC;
    bottom[1] &= ~0x3F;
    top[JUDGE_W - 2] &= ~0xFC;
    bottom[JUDGE_W - 2] &= ~0x3F;
    draw_action_label_16(top, bottom, true);
    return;
  }

  for (uint8_t x = 2; x < JUDGE_W - 2; x++) {
    top[x] |= 0x02;
    bottom[x] |= 0x40;
  }
  top[1] |= 0xFC;
  bottom[1] |= 0x3F;
  top[JUDGE_W - 2] |= 0xFC;
  bottom[JUDGE_W - 2] |= 0x3F;
  draw_action_label_16(top, bottom, false);
}

static void draw_input_header() {
  draw_text(32, 1, MSG_INPUT_HEADER, sizeof(MSG_INPUT_HEADER));
}

static void draw_chances() {
  SSD1306.ssd1306_setpos(104, 0);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t i = 0; i < MAX_WRONG; i++) {
    for (uint8_t c = 0; c < 7; c++) {
      uint8_t b = (i >= wrong_count) ? pgm_read_byte(&HEART_GLYPH[c]) : 0x00;
      SSD1306.ssd1306_send_byte(b);
    }
    SSD1306.ssd1306_send_byte(0x00);
  }
  SSD1306.ssd1306_send_data_stop();
}

static void draw_heart(uint8_t idx, bool on) {
  SSD1306.ssd1306_setpos(104 + idx * 8, 0);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < 7; c++) {
    SSD1306.ssd1306_send_byte(on ? pgm_read_byte(&HEART_GLYPH[c]) : 0x00);
  }
  SSD1306.ssd1306_send_data_stop();
}

static void shake_offset(uint8_t line) {
  SSD1306.ssd1306_send_command(OLED_CMD_START_LINE | line);
}

// Quiz-style "wrong!" buzzer synced with a screen jolt.
// The start-line shake wraps the top rows to the bottom edge, so the hearts
// (the only content in page 0) are hidden during the jolt and restored after.
static void error_feedback() {
  for (uint8_t i = 0; i < MAX_WRONG; i++) draw_heart(i, false);

  shake_offset(3);
  play_note(NOTE_BUZZER, 110);
  shake_offset(0);
  play_note(NOTE_REST, 70);

  shake_offset(5);
  play_note(NOTE_BUZZER_LO, 140);
  shake_offset(2);
  play_note(NOTE_BUZZER_LO, 140);
  shake_offset(0);
  play_note(NOTE_BUZZER_LO, 160);

  // Restore hearts as they were before this loss; blink_lost_heart() follows
  for (uint8_t i = 0; i < MAX_WRONG; i++) {
    draw_heart(i, i >= (uint8_t)(wrong_count - 1));
  }
}

// The heart lost by this mistake (index wrong_count-1) blinks out
static void blink_lost_heart() {
  uint8_t idx = wrong_count - 1;
  for (uint8_t i = 0; i < 3; i++) {
    draw_heart(idx, false);
    wait_ms(90);
    draw_heart(idx, true);
    wait_ms(90);
  }
  draw_heart(idx, false);
  wait_ms(250);
}

static void draw_slot(uint8_t i) {
  uint8_t x = SLOT_X0 + i * SLOT_STEP;
  uint8_t gid = slot_glyph_id(i);
  uint8_t top[SLOT_BOX_W] = {0};
  uint8_t bottom[SLOT_BOX_W] = {0};
  compose_glyph_2x(top, bottom, 0, &KANA_GLYPHS[gid][0]);

  if ((cursor_idx == i) && !edit_mode) {
    for (uint8_t c = 0; c < SLOT_BOX_W; c++) bottom[c] |= 0x80;
  }

  if (((cursor_idx == i) && edit_mode) || (force_invert_slot == i)) {
    for (uint8_t c = 0; c < SLOT_BOX_W; c++) {
      top[c] = ~top[c];
      bottom[c] = ~bottom[c];
    }
  }

  send_16px(x, SLOT_PAGE, top, bottom, SLOT_BOX_W);
}

static void draw_judge() {
  uint8_t top[JUDGE_W] = {0};
  uint8_t bottom[JUDGE_W] = {0};
  if (all_filled()) {
    draw_action_button_16(top, bottom, cursor_idx == CURSOR_JUDGE);
  }

  send_16px(JUDGE_X, JUDGE_PAGE, top, bottom, JUDGE_W);
}

static void move_caret(uint8_t from, uint8_t to) {
  if (from < 4) draw_slot(from);
  else draw_judge();
  if (to < 4) draw_slot(to);
  else draw_judge();
}

static void draw_input_content() {
  draw_input_header();
  draw_chances();
  for (uint8_t i = 0; i < 4; i++) draw_slot(i);
  draw_judge();
}

static void draw_input_full() {
  clear_screen();
  draw_input_content();
}

static void show_title() {
  SSD1306.ssd1306_tiny_init();
  ssd1306_ready = true;
  for (uint8_t page = 0; page < 8; page++) {
    SSD1306.ssd1306_setpos(0, page);
    SSD1306.ssd1306_send_data_start();
    for (uint8_t x = 0; x < 128; x++) {
      SSD1306.ssd1306_send_byte(pgm_read_byte(&TITLE_INTRO[(uint16_t)page * 128 + x]));
    }
    SSD1306.ssd1306_send_data_stop();
  }
}

static void start_game() {
  reset_input();
  state = STATE_INPUT;
  present_screen(draw_input_full);
  poll_buttons();
  prev_l = left_pressed();
  prev_r = right_pressed();
  prev_u = up_pressed();
  prev_d = down_pressed();
  prev_f = fire_pressed();
}

static void draw_msg_ng() {
  clear_screen();
  draw_text_16(40, 2, MSG_NG_TITLE, sizeof(MSG_NG_TITLE));
  draw_glyph_bytes_16(72, 2, MSG_LONG_MARK);

  uint8_t x = 21;
  uint8_t remain = MAX_WRONG - wrong_count;
  draw_glyph(x, 5, GLYPH_CHI); x += 8;
  draw_glyph_bytes(x, 5, MSG_SMALL_YA); x += 8;
  draw_text(x, 5, MSG_NG_MID, sizeof(MSG_NG_MID)); x += sizeof(MSG_NG_MID) * 8;
  draw_glyph_bytes(x, 5, MSG_TOUTEN); x += 8;
  draw_text(x, 5, MSG_NG_AFTER_DIGIT, sizeof(MSG_NG_AFTER_DIGIT)); x += sizeof(MSG_NG_AFTER_DIGIT) * 8;
  draw_digit6(x, 5, remain); x += 6;
  draw_glyph(x, 5, GLYPH_KA); x += 8;
  draw_glyph(x, 5, GLYPH_I);
}

static void draw_msg_clear() {
  clear_screen();
  draw_text_16(40, 2, MSG_CLEAR_TITLE, sizeof(MSG_CLEAR_TITLE));
  draw_text(32, 5, MSG_CLEAR_SUB, sizeof(MSG_CLEAR_SUB));
}

static void draw_msg_over() {
  clear_screen();
  uint8_t x16 = 8;
  draw_glyph_16(x16, 3, GLYPH_GE); x16 += 16;
  draw_glyph_bytes_16(x16, 3, MSG_LONG_MARK); x16 += 16;
  draw_glyph_16(x16, 3, GLYPH_MU); x16 += 16;
  draw_glyph_16(x16, 3, GLYPH_O); x16 += 16;
  draw_glyph_bytes_16(x16, 3, MSG_LONG_MARK); x16 += 16;
  draw_glyph_16(x16, 3, GLYPH_BA); x16 += 16;
  draw_glyph_bytes_16(x16, 3, MSG_LONG_MARK);
}

static void draw_blank8(uint8_t x, uint8_t page) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < 8; c++) SSD1306.ssd1306_send_byte(0x00);
  SSD1306.ssd1306_send_data_stop();
}

static void draw_sparkles(uint8_t phase) {
  for (uint8_t i = 0; i < SPARKLE_COUNT; i++) {
    uint8_t x = pgm_read_byte(&SPARKLE_POS[i][0]);
    uint8_t page = pgm_read_byte(&SPARKLE_POS[i][1]);
    if ((i & 1) == phase) draw_glyph_bytes(x, page, SPARKLE_GLYPH);
    else draw_blank8(x, page);
  }
}

static void celebrate_clear() {
  for (uint8_t i = 0; i < 2; i++) {
    SSD1306.ssd1306_send_command(OLED_CMD_INVERT);
    wait_ms(70);
    SSD1306.ssd1306_send_command(OLED_CMD_NORMAL);
    wait_ms(70);
  }

  for (uint8_t n = 0; n < CLEAR_MELODY_LEN; n++) {
    uint16_t half = pgm_read_word(&CLEAR_MELODY[n][0]);
    uint16_t ms = pgm_read_word(&CLEAR_MELODY[n][1]);
    if (half != 0) draw_sparkles(n & 1);
    play_note(half, ms);
  }
}

static void gameover_sequence() {
  // Stage 1: alarm flashes over the broken input screen
  for (uint8_t i = 0; i < 4; i++) {
    SSD1306.ssd1306_send_command(OLED_CMD_INVERT);
    play_note(NOTE_ALARM_HI, 130);
    SSD1306.ssd1306_send_command(OLED_CMD_NORMAL);
    play_note(NOTE_ALARM_LO, 170);
  }

  // Stage 2: invaders erode the screen, accelerating
  uint8_t wait = 70;
  for (uint8_t i = 0; i < 56; i++) {
    uint8_t x = (rnd() & 0x0F) << 3;
    uint8_t page = rnd() & 0x07;
    draw_glyph_bytes(x, page, INVADER_GLYPH);
    play_note(250 + ((uint16_t)(rnd() & 0x3F) << 4), 14);
    wait_ms(wait);
    if (wait > 4) wait -= 2;
  }

  SSD1306.ssd1306_send_command(OLED_CMD_INVERT);
  play_note(NOTE_ALARM_LO, 140);
  SSD1306.ssd1306_send_command(OLED_CMD_NORMAL);
  clear_screen();
  wait_ms(500);

  // Stage 3: the end — descending doom sweep into a low rumble
  present_screen(draw_msg_over);
  for (uint16_t half = 500; half <= 2300; half += 90) {
    play_note(half, 30);
  }
  play_note(2400, 500);
  wait_ms(120);
  play_note(2600, 700);
}

// Drum-roll suspense: each slot gets "scanned" left to right before the verdict
static void judge_suspense() {
  for (uint8_t i = 0; i < 4; i++) {
    force_invert_slot = i;
    draw_slot(i);
    for (uint8_t k = 0; k < 6; k++) {
      play_note(1500 + ((uint16_t)(rnd() & 0x07) << 5), 12);
      wait_ms(8);
    }
    force_invert_slot = 0xFF;
    draw_slot(i);
  }
  wait_ms(220);
}

static void on_judge() {
  if (!all_filled()) {
    return;
  }

  judge_suspense();

  if (matches_correct()) {
    state = STATE_CLEAR;
    present_screen(draw_msg_clear);
    screen_dirty = false;
    celebrate_clear();
    return;
  }

  wrong_count++;
  if (wrong_count >= MAX_WRONG) {
    state = STATE_GAMEOVER;
    gameover_sequence();
    screen_dirty = false;
    return;
  } else {
    state = STATE_ERROR;
    error_feedback();
    blink_lost_heart();
    present_screen(draw_msg_ng);
  }
  screen_dirty = false;
}

// Keep the entered characters so the player can tweak and retry
static void return_from_ng() {
  cursor_idx = 0;
  last_slot_idx = 0;
  drum_hold_dir = 0;
  drum_repeat_timer = 0;
  edit_mode = false;
  state = STATE_INPUT;
  mark_dirty();
}

// FIRE on clear/game over screens hands the device to the next player
static void return_to_title() {
  while (fire_pressed()) {
    _delay_ms(IDLE_MS);
  }
  show_title();
  title_tick = TITLE_FIRST_BLINK;
  state = STATE_TITLE;
}

static void move_left() {
  uint8_t from = cursor_idx;
  if (edit_mode) edit_mode = false;
  if (cursor_idx == CURSOR_JUDGE) cursor_idx = 3;
  else cursor_idx = (cursor_idx == 0) ? 3 : (cursor_idx - 1);
  last_slot_idx = cursor_idx;
  move_caret(from, cursor_idx);
}

static void move_right() {
  uint8_t from = cursor_idx;
  if (edit_mode) edit_mode = false;
  if (cursor_idx == CURSOR_JUDGE) cursor_idx = 0;
  else cursor_idx = (cursor_idx >= 3) ? 0 : (cursor_idx + 1);
  last_slot_idx = cursor_idx;
  move_caret(from, cursor_idx);
}

static void move_up() {
  if (cursor_idx != CURSOR_JUDGE) return;
  uint8_t from = cursor_idx;
  cursor_idx = last_slot_idx;
  move_caret(from, cursor_idx);
}

static void move_down() {
  if (cursor_idx >= 4) return;
  if (!all_filled()) return;
  uint8_t from = cursor_idx;
  last_slot_idx = cursor_idx;
  cursor_idx = CURSOR_JUDGE;
  move_caret(from, cursor_idx);
}

static void drum_step(int8_t delta) {
  if (cursor_idx >= 4) return;
  uint8_t p = drum_pos[cursor_idx];
  if (slots[cursor_idx] == SLOT_EMPTY) {
    p = (delta < 0) ? (KANA_DRUM_LEN - 1) : 0;
  } else if (delta < 0) {
    p = (p == 0) ? (KANA_DRUM_LEN - 1) : (p - 1);
  } else {
    p = (p + 1) % KANA_DRUM_LEN;
  }
  drum_pos[cursor_idx] = p;
  slots[cursor_idx] = p;
  sound(200, 3);  // dial-lock click per notch
  draw_slot(cursor_idx);
  draw_judge();
}

static void drum_up() {
  drum_step(-1);
}

static void drum_down() {
  drum_step(1);
}

static void update_drum_hold() {
  if (cursor_idx >= 4) {
    drum_hold_dir = 0;
    drum_repeat_timer = 0;
    return;
  }

  uint8_t dir = 0;
  if (up_pressed()) dir = 1;
  else if (down_pressed()) dir = 2;

  if (dir == 0) {
    drum_hold_dir = 0;
    drum_repeat_timer = 0;
    return;
  }

  if (dir != drum_hold_dir) {
    drum_hold_dir = dir;
    drum_repeat_timer = DRUM_HOLD_DELAY;
    if (dir == 1) drum_up();
    else drum_down();
    return;
  }

  if (drum_repeat_timer > 0) {
    drum_repeat_timer--;
    return;
  }

  drum_repeat_timer = DRUM_REPEAT_EVERY;
  if (dir == 1) drum_up();
  else drum_down();
}

static void on_fire() {
  if (cursor_idx == CURSOR_JUDGE) {
    on_judge();
    return;
  }

  edit_mode = !edit_mode;
  drum_hold_dir = 0;
  drum_repeat_timer = 0;
  if (edit_mode) play_note(700, 18);  // open
  else play_note(450, 25);            // confirm
  draw_slot(cursor_idx);
}

static void edge_run(bool now, bool* prev, void (*fn)()) {
  if (now && !(*prev)) fn();
  *prev = now;
}

void setup() {
  pinMode(1, INPUT);
  pinMode(4, OUTPUT);
  pinMode(A0, INPUT);

  show_title();
  title_tick = TITLE_FIRST_BLINK;

  state = STATE_TITLE;
}

void loop() {
  if (state == STATE_TITLE) {
    if (digitalRead(1) == 0) {
      sound_start();
      pinMode(A3, INPUT);
      start_game();
      return;
    }

    // Attract: double blink every ~4 s to invite passersby
    title_tick++;
    if (title_tick >= TITLE_BLINK_EVERY) {
      for (uint8_t i = 0; i < 2; i++) {
        SSD1306.ssd1306_send_command(OLED_CMD_INVERT);
        _delay_ms(60);
        SSD1306.ssd1306_send_command(OLED_CMD_NORMAL);
        _delay_ms(60);
      }
      title_tick = 0;
    }

    _delay_ms(IDLE_MS);
    return;
  }

  poll_buttons();

  if (state == STATE_INPUT) {
    edge_run(left_pressed(), &prev_l, move_left);
    edge_run(right_pressed(), &prev_r, move_right);
    if (edit_mode) {
      update_drum_hold();
      prev_u = up_pressed();
      prev_d = down_pressed();
    } else {
      drum_hold_dir = 0;
      drum_repeat_timer = 0;
      edge_run(up_pressed(), &prev_u, move_up);
      edge_run(down_pressed(), &prev_d, move_down);
    }
    edge_run(fire_pressed(), &prev_f, on_fire);

    // Last life: the remaining heart pulses
    if (wrong_count == MAX_WRONG - 1) {
      pulse_tick++;
      if ((pulse_tick & 0x1F) == 0) draw_heart(MAX_WRONG - 1, true);
      else if ((pulse_tick & 0x1F) == 16) draw_heart(MAX_WRONG - 1, false);
    }
  } else if (state == STATE_ERROR) {
    edge_run(fire_pressed(), &prev_f, return_from_ng);
  } else {
    edge_run(fire_pressed(), &prev_f, return_to_title);
    if (state == STATE_TITLE) return;
  }

  if (screen_dirty) {
    if (state == STATE_INPUT) {
      present_screen(draw_input_full);
    } else if (state == STATE_ERROR) {
      present_screen(draw_msg_ng);
    } else if (state == STATE_CLEAR) {
      present_screen(draw_msg_clear);
    } else {
      present_screen(draw_msg_over);
    }
    screen_dirty = false;
  }

  _delay_ms(IDLE_MS);
}
