# 解読せよ！電子基板クエスト

TinyJoypad / ATtiny85 向けの4文字解読ゲーム用ファームウェアです。SSD1306 OLED、5ボタン、ブザーを使い、会場内で集めた4文字の暗号を入力してクリア判定します。

## 現在の仕様

- 起動直後にタイトル画像を表示
- 4文字のカナ入力
- 入力可能文字は清音、濁音、半濁音、数字
- 正解時はクリア画面、失敗時はエラー画面
- ライフは3つ。3回失敗でゲームオーバー
- ライフ表示は右上のハート
- タイトル、入力、エラー、クリア、ゲームオーバーに効果音あり
- EEPROM保存なし。電源OFF/ONで初期状態に戻る

## 操作

| 操作 | 入力画面での動き |
|------|------------------|
| LEFT / RIGHT | 4つの文字スロット間を移動 |
| UP / DOWN | 編集モード中にカナホイールを回す |
| FIRE | スロット選択時は編集開始/確定。カイドク!選択時は判定 |

4文字すべてが埋まるまで `カイドク!` ボタンは表示されません。

## 画面構成

入力画面はSSD1306の8ページ構成を意識して配置しています。

| ページ | 内容 |
|--------|------|
| page0 | ハートゲージ |
| page1 | `アンゴウヲイレヨ` |
| page2 | 余白 |
| page3-4 | 16px文字スロット |
| page5 | 余白 |
| page6-7 | `カイドク!` ボタン |

タイトル画面は `tools/gen_title_intro.py` から `firmware/pcb_quest/title_intro.h` を生成します。

## 構成

```text
pcb-quest/
├── firmware/pcb_quest/
│   ├── pcb_quest.ino      # メインファームウェア
│   ├── answer.h           # 正解4文字
│   ├── kana_table.h       # カナホイール順序
│   ├── kana_glyphs.h      # 8x8グリフ
│   └── title_intro.h      # 起動画面ビットマップ
├── tools/
│   ├── gen_kana_glyphs.py
│   ├── gen_title_intro.py
│   ├── grid_image_to_bitmap.py
│   └── misaki_font_f0.h
├── scripts/
│   ├── burn_fuses_16mhz_isp.sh
│   └── flash_isp.sh
└── docs/
    └── venue_template.md
```

`assets/` と `backups/` はローカル作業用としてGit管理外です。

## ビルド

```bash
python3 tools/gen_kana_glyphs.py
python3 tools/gen_title_intro.py
arduino-cli compile --fqbn attiny:avr:ATtinyX5:cpu=attiny85,clock=internal16 firmware/pcb_quest
```

## 書き込み

初回、または動作が極端に遅い場合は、16MHz用のヒューズを書きます。

```bash
bash scripts/burn_fuses_16mhz_isp.sh arduinoasisp /dev/cu.usbserial-A5069RR4
```

通常のファーム更新:

```bash
bash scripts/flash_isp.sh arduinoasisp /dev/cu.usbserial-A5069RR4
```

直接 `arduino-cli` を使う場合:

```bash
arduino-cli upload \
  --fqbn attiny:avr:ATtinyX5:cpu=attiny85,clock=internal16 \
  --port /dev/cu.usbserial-A5069RR4 \
  --programmer arduinoasisp \
  firmware/pcb_quest
```

## クロック設定

このファームはATtiny85の内蔵16MHz動作を前提にしています。表示や音、待ち時間が約16倍遅く感じる場合は、低レベルヒューズが16MHz設定になっていない可能性があります。

確認値:

```text
lfuse = 0xF1
```

## 正解の変更

`firmware/pcb_quest/answer.h` の `CORRECT_ANSWER` を編集します。

現在は検証しやすいように `アアアア` です。

```cpp
static const uint8_t CORRECT_ANSWER[4] PROGMEM = {0, 0, 0, 0};
```

インデックスは `firmware/pcb_quest/kana_table.h` の順序です。

## 実機チェック

- 電源ON直後にタイトルが表示される
- タイトルでFIREを押すと起動音が鳴り、入力画面へ遷移する
- スロット選択時にFIREで編集モードへ入る
- UP/DOWN単発で1文字移動、長押しで高速移動する
- 4文字すべて埋まると `カイドク!` が表示される
- 不正解でエラー画面、FIREで入力画面へ戻る
- 3回不正解でゲームオーバー
- 正解でクリア画面
