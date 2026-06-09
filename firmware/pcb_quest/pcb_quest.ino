// 解読せよ！電子基盤クエスト — MVP
// TinyJoypad ATtiny85 @ 16 MHz internal
#include <ssd1306xled.h>
#include <Arduino.h>

#include "answer.h"
#include "kana_glyphs.h"
#include "kana_table.h"
#include "title_intro.h"

#define MAX_WRONG 3
#define SLOT_EMPTY 0xFF
#define CURSOR_JUDGE 4
#define IDLE_MS 16
#define DRUM_HOLD_DELAY 12
#define DRUM_REPEAT_EVERY 1

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

static const uint8_t MSG_SMALL_TSU[8] PROGMEM = {
  0x00, 0x00, 0x0C, 0x14, 0x40, 0x20, 0x1C, 0x00
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
static uint8_t blink_frames;
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

static void boot_i2c_init() {
  DDRB |= _BV(PB0) | _BV(PB2);
  PORTB |= _BV(PB0) | _BV(PB2);
}

static void boot_i2c_start() {
  PORTB |= _BV(PB0) | _BV(PB2);
  PORTB &= ~_BV(PB0);
  PORTB &= ~_BV(PB2);
}

static void boot_i2c_stop() {
  PORTB &= ~_BV(PB0);
  PORTB |= _BV(PB2);
  PORTB |= _BV(PB0);
}

static void boot_i2c_write(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    PORTB &= ~_BV(PB2);
    if (data & 0x80) PORTB |= _BV(PB0);
    else PORTB &= ~_BV(PB0);
    PORTB |= _BV(PB2);
    data <<= 1;
  }

  PORTB &= ~_BV(PB2);
  PORTB |= _BV(PB0);
  DDRB &= ~_BV(PB0);
  PORTB |= _BV(PB2);
  PORTB &= ~_BV(PB2);
  DDRB |= _BV(PB0);
  PORTB &= ~_BV(PB0);
}

static void boot_cmd_start() {
  boot_i2c_start();
  boot_i2c_write(0x3C << 1);
  boot_i2c_write(0x00);
}

static void boot_data_start() {
  boot_i2c_start();
  boot_i2c_write(0x3C << 1);
  boot_i2c_write(0x40);
}

static void boot_display_init_on() {
  static const uint8_t init[] PROGMEM = {
    0xAE,
    0xD5, 0xF0,
    0xA8, 0x3F,
    0xD3, 0x00,
    0x40,
    0x8D, 0x14,
    0x20, 0x02,
    0xA1,
    0xC8,
    0xDA, 0x12,
    0x81, 0x3F,
    0xD9, 0x22,
    0xDB, 0x20,
    0xA4,
    0xA6,
    0x2E,
    0xAF
  };

  boot_i2c_init();
  boot_cmd_start();
  for (uint8_t i = 0; i < sizeof(init); i++) {
    boot_i2c_write(pgm_read_byte(&init[i]));
  }
  boot_i2c_stop();
}

static void boot_title_paint() {
  for (uint8_t page = 0; page < 8; page++) {
    boot_cmd_start();
    boot_i2c_write(0xB0 | page);
    boot_i2c_write(0x00);
    boot_i2c_write(0x10);
    boot_i2c_stop();

    boot_data_start();
    for (uint8_t x = 0; x < 128; x++) {
      boot_i2c_write(pgm_read_byte(&TITLE_INTRO[(uint16_t)page * 128 + x]));
    }
    boot_i2c_stop();
  }
}

static void boot_show_title() {
  boot_display_init_on();
  boot_title_paint();
}

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

static void sound_error() {
  for (uint8_t i = 0; i < 4; i++) {
    sound(70, 8);
    sound(115, 6);
    _delay_ms(28);
  }
}

static void sound_clear() {
  sound(120, 80);
  _delay_ms(25);
  sound(175, 90);
  _delay_ms(25);
  sound(220, 170);
}

static void sound_gameover() {
  for (uint8_t i = 0; i < 4; i++) {
    sound((i & 1) ? 80 : 180, 70);
    _delay_ms(45);
  }
  _delay_ms(40);
  sound(35, 220);
}

static void reset_game_data() {
  for (uint8_t i = 0; i < 4; i++) {
    slots[i] = SLOT_EMPTY;
    drum_pos[i] = 0;
  }
  cursor_idx = 0;
  last_slot_idx = 0;
  wrong_count = 0;
  blink_frames = 0;
  drum_hold_dir = 0;
  drum_repeat_timer = 0;
  edit_mode = false;
}

static void clear_slots() {
  for (uint8_t i = 0; i < 4; i++) slots[i] = SLOT_EMPTY;
}

static void reset_input_slots() {
  clear_slots();
  for (uint8_t i = 0; i < 4; i++) drum_pos[i] = 0;
  cursor_idx = 0;
  last_slot_idx = 0;
  blink_frames = 0;
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

static void draw_glyph(uint8_t x, uint8_t page, uint8_t glyph_id, bool invert) {
  SSD1306.ssd1306_setpos(x, page);
  SSD1306.ssd1306_send_data_start();
  for (uint8_t c = 0; c < GLYPH_BYTES; c++) {
    uint8_t b = pgm_read_byte(&KANA_GLYPHS[glyph_id][c]);
    SSD1306.ssd1306_send_byte(invert ? ~b : b);
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
  const uint8_t header[] = {0, 45, GLYPH_GO, 2, 44, 1, 41, 37};
  uint8_t x = 32;
  for (uint8_t i = 0; i < 8; i++) {
    draw_glyph(x, 1, header[i], false);
    x += 8;
  }
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

static void draw_slot(uint8_t i) {
  uint8_t x = SLOT_X0 + i * SLOT_STEP;
  uint8_t gid = slot_glyph_id(i);
  uint8_t top[SLOT_BOX_W] = {0};
  uint8_t bottom[SLOT_BOX_W] = {0};
  compose_glyph_2x(top, bottom, 0, &KANA_GLYPHS[gid][0]);

  if ((cursor_idx == i) && !edit_mode) {
    for (uint8_t c = 0; c < SLOT_BOX_W; c++) bottom[c] |= 0x80;
  }

  if (((cursor_idx == i) && edit_mode) || (slots[i] == SLOT_EMPTY && blink_frames > 0 && blink_frames < 4)) {
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

static void start_game() {
  if (!ssd1306_ready) {
    SSD1306.ssd1306_tiny_init();
    ssd1306_ready = true;
  }
  reset_game_data();
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
  draw_glyph_16(40, 2, 3);                              // エ
  draw_glyph_16(56, 2, 38);                             // ラ
  draw_glyph_bytes_16(72, 2, MSG_LONG_MARK);

  uint8_t x = 21;
  uint8_t remain = MAX_WRONG - wrong_count;
  draw_glyph(x, 5, 16, false); x += 8;                // チ
  draw_glyph_bytes(x, 5, MSG_SMALL_YA); x += 8;       // ャ
  draw_glyph(x, 5, 45, false); x += 8;                // ン
  draw_glyph(x, 5, 12, false); x += 8;                // ス
  draw_glyph(x, 5, 25, false); x += 8;                // ハ
  draw_glyph_bytes(x, 5, MSG_TOUTEN); x += 8;         // 、
  draw_glyph(x, 5, 0, false); x += 8;                 // ア
  draw_glyph(x, 5, 19, false); x += 8;                // ト
  draw_digit6(x, 5, remain); x += 6;
  draw_glyph(x, 5, 5, false); x += 8;                 // カ
  draw_glyph(x, 5, 1, false);                         // イ
}

static void draw_msg_clear() {
  clear_screen();
  draw_glyph_16(40, 2, 7);                              // ク
  draw_glyph_16(56, 2, 39);                             // リ
  draw_glyph_16(72, 2, 0);                              // ア

  uint8_t x = 32;
  draw_glyph(x, 5, 5, false); x += 8;                 // カ
  draw_glyph(x, 5, 1, false); x += 8;                 // イ
  draw_glyph_bytes(x, 5, ACTION_GLYPHS[2]); x += 8;   // ド
  draw_glyph(x, 5, 7, false); x += 8;                 // ク
  draw_glyph(x, 5, 13, false); x += 8;                // セ
  draw_glyph(x, 5, 1, false); x += 8;                 // イ
  draw_glyph(x, 5, 9, false); x += 8;                 // コ
  draw_glyph(x, 5, 2, false);                         // ウ
}

static void draw_msg_over() {
  clear_screen();
  uint8_t x16 = 8;
  draw_glyph_16(x16, 2, 49); x16 += 16;               // ゲ
  draw_glyph_bytes_16(x16, 2, MSG_LONG_MARK); x16 += 16;
  draw_glyph_16(x16, 2, 32); x16 += 16;               // ム
  draw_glyph_16(x16, 2, 4); x16 += 16;                // オ
  draw_glyph_bytes_16(x16, 2, MSG_LONG_MARK); x16 += 16;
  draw_glyph_16(x16, 2, 61); x16 += 16;               // バ
  draw_glyph_bytes_16(x16, 2, MSG_LONG_MARK);

  uint8_t x = 32;
  draw_glyph(x, 5, 5, false); x += 8;                 // カ
  draw_glyph(x, 5, 1, false); x += 8;                 // イ
  draw_glyph_bytes(x, 5, ACTION_GLYPHS[2]); x += 8;   // ド
  draw_glyph(x, 5, 7, false); x += 8;                 // ク
  draw_glyph(x, 5, 11, false); x += 8;                // シ
  draw_glyph_bytes(x, 5, MSG_SMALL_TSU); x += 8;      // ッ
  draw_glyph(x, 5, 66, false); x += 8;                // パ
  draw_glyph(x, 5, 1, false);                         // イ
}

static void on_judge() {
  if (!all_filled()) {
    return;
  }

  if (matches_correct()) {
    state = STATE_CLEAR;
    sound_clear();
    present_screen(draw_msg_clear);
    screen_dirty = false;
    return;
  }

  wrong_count++;
  if (wrong_count >= MAX_WRONG) {
    state = STATE_GAMEOVER;
    sound_gameover();
    present_screen(draw_msg_over);
  } else {
    state = STATE_ERROR;
    sound_error();
    present_screen(draw_msg_ng);
  }
  screen_dirty = false;
}

static void return_from_ng() {
  reset_input_slots();
  state = STATE_INPUT;
  mark_dirty();
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

  boot_show_title();

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

    if (blink_frames > 0) {
      bool was_on = blink_frames < 4;
      blink_frames--;
      bool now_on = (blink_frames > 0) && (blink_frames < 4);
      if (was_on != now_on) {
        for (uint8_t i = 0; i < 4; i++) {
          if (slots[i] == SLOT_EMPTY) draw_slot(i);
        }
      }
    }
  } else if (state == STATE_ERROR) {
    edge_run(fire_pressed(), &prev_f, return_from_ng);
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
