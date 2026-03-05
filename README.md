# tcxIME

Native OS IME (Input Method Editor) addon for [TrussC](https://github.com/toru-music/trussc).
Enables Japanese / CJK text input with kanji conversion, inline marked-text rendering, and automatic word wrapping.

> **Platform support:** macOS / Windows (full IME). Linux compiles with stubs but IME is not yet functional.

---

## 概要

tcxIME は TrussC 用の IME アドオンで、macOS / Windows のネイティブ日本語入力（かな漢字変換）をアプリに組み込める。

- ひらがな入力 → 漢字変換 → 確定 の一連のフローをサポート
- 変換候補ウィンドウの表示
- マルチライン対応（Enter で改行、上下矢印で行移動）
- `maxWidth` 指定による自動折り返し（確定テキスト・変換中テキスト両対応）
- テキスト選択（クリック、ドラッグ、Shift+矢印、Cmd/Ctrl+A）
- コピー（Cmd/Ctrl+C）、カット（Cmd/Ctrl+X）、ペースト（Cmd/Ctrl+V）
- 英数 / かな モードの OS 同期
- **Node ベースのウィジェット**: TextField, FloatField, IntField 等
- **排他制御**: 複数フィールドがあっても、アクティブな IME は常に 1 つだけ

## セットアップ

### addons.make

プロジェクトの `addons.make` に追加:

```
tcxIME
```

その後 projectGenerator で `--update` すれば CMake に反映される。

### 手動リンク

```cmake
add_subdirectory(path/to/tcxIME)
target_link_libraries(myApp PRIVATE tcxIME)
```

---

## 使い方 1: tcxIME を直接使う

Node を使わず、`draw()` の中で直接描画するシンプルな方式。

```cpp
#include <TrussC.h>
#include <tcxIME.h>
using namespace std;
using namespace tc;

class tcApp : public App {
    tcxIME ime_;

    void setup() override {
        ime_.setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 24);
        ime_.setMaxWidth(600);   // 折り返し幅
        ime_.enable();
    }

    void draw() override {
        background(0.1f);
        setColor(1.0f);
        ime_.draw(40, 40);
    }
};
```

`ime_.getString()` で確定済みテキストを UTF-8 で取得できる。

---

## 使い方 2: TextField（Node ベース）

`tcxIME::TextField` は RectNode を継承したウィジェット。マウスクリック・ドラッグによるカーソル操作と選択が自動で動く。

```cpp
#include <TrussC.h>
#include <tcxIME.h>
using namespace std;
using namespace tc;

class tcApp : public App {
    shared_ptr<tcxIME::TextField> textField_;

    void setup() override {
        textField_ = make_shared<tcxIME::TextField>();
        textField_->setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 24);
        textField_->setPos(20, 40);
        textField_->setSize(760, 300);  // setSize で maxWidth も連動
        textField_->enable();
        addChild(textField_);
    }

    void draw() override {
        clear(0.1f);
    }
};
```

**特徴:**
- `setSize()` で RectNode の幅と `maxWidth` が自動連動
- テキスト形状の当たり判定（テキストがある箇所だけヒット、複数行で凸凹）
- クリックでカーソル配置、ドラッグで範囲選択
- `setEnableNewLine(false)` で単一行入力に変更可
- `setInputFilter()` で入力文字をフィルタリング

---

## 使い方 3: FloatField / IntField（数値入力）

レンジ付きの数値入力ウィジェット。Enter で値を確定し、範囲外の値は自動クランプ。

```cpp
// Float 入力 [-100, 100]
auto floatField = make_shared<tcxIME::FloatField>();
floatField->setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 20);
floatField->setPos(20, 40);
floatField->setSize(200, 30);
floatField->setRange(-100.0f, 100.0f);
floatField->setValue(3.14f);
floatField->onValueChanged = [](float v) {
    logNotice() << "Float: " << v;
};
floatField->enable();
addChild(floatField);

// Int 入力 [0, 255]
auto intField = make_shared<tcxIME::IntField>();
intField->setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 20);
intField->setPos(20, 100);
intField->setSize(200, 30);
intField->setRange(0, 255);
intField->setValue(128);
intField->onValueChanged = [](int v) {
    logNotice() << "Int: " << v;
};
intField->enable();
addChild(intField);
```

### 型バリエーション

テンプレートで精度を選べる:

| エイリアス | 内部テンプレート | 用途 |
|-----------|-----------------|------|
| `tcxIME::FloatField` | `FloatingPointField_<float>` | 通常の小数値 |
| `tcxIME::DoubleField` | `FloatingPointField_<double>` | 倍精度小数値 |
| `tcxIME::IntField` | `IntegerField_<int>` | 通常の整数 |
| `tcxIME::Int64Field` | `IntegerField_<int64_t>` | 64bit 整数 |

### 数値フィールド共通 API

| メソッド | 説明 |
|---------|------|
| `setValue(T)` | 値をセット（レンジでクランプ） |
| `getValue()` | 現在の値を取得 |
| `setRange(min, max)` | 上下限を設定 |
| `setMin(T)` / `setMax(T)` | 片方だけ設定 |
| `clearMin()` / `clearMax()` / `clearRange()` | 制限を解除 |
| `onValueChanged` | Enter 確定時のコールバック `function<void(T)>` |
| `getAsFloat()` / `getAsDouble()` | FloatField 系の型変換取得 |
| `getAsInt()` / `getAsInt64()` | IntField 系の型変換取得 |

**動作:**
- 入力は数値文字のみ許可（自動 inputFilter）
- Enter で文字列をパース → クランプ → 表示更新 → `onValueChanged` 発火
- 矩形の当たり判定（空でもクリック可能）

---

## 排他制御

複数の IME フィールドがある場合、`enable()` を呼ぶと前にアクティブだった IME は自動で `disable()` される。同時に複数の IME がアクティブになることはない。

`disable()` されたフィールドは選択範囲もクリアされる。

---

## API リファレンス

### tcxIME（メインクラス）

| メソッド | 説明 |
|---------|------|
| `setFont(path, size)` | フォントをファイルから読み込み |
| `setFont(Font*)` | 既存の Font を共有 |
| `setMaxWidth(float)` | 自動折り返し幅を設定（0 で無効） |
| `enable()` | IME 入力を有効化（他の IME は自動 disable） |
| `disable()` | IME 入力を無効化（選択もクリア） |
| `clear()` | テキストを全消去 |
| `draw(x, y)` | テキスト・カーソル・変換候補を描画 |
| `getString()` | 確定テキストを UTF-8 で取得 |
| `setString(str)` | UTF-8 文字列をセット |
| `isEnabled()` | 有効状態を返す |
| `isJapaneseMode()` | かな/変換中なら true |
| `setEnableNewLine(bool)` | 改行の有効/無効（デフォルト有効） |
| `getEnableNewLine()` | 改行の有効状態を取得 |
| `inputFilter` | `function<bool(char32_t)>` 入力文字フィルタ |
| `onEnter` | `function<void()>` Enter キーコールバック |
| `setCursorFromMouse(x, y)` | マウス位置にカーソルを設定 |
| `extendSelectionFromMouse(x, y)` | マウス位置まで選択を拡張 |
| `getSelectedText()` | 選択中のテキストを UTF-8 で取得 |
| `getLineCount()` | 行数を取得 |
| `getLineWidth(line)` | 指定行の描画幅を取得 |
| `getLineHeight()` | 1 行の高さを取得 |

### キーバインド

| キー | 機能 |
|------|------|
| Cmd+A (macOS) / Ctrl+A (Win) | 全選択 |
| Cmd+C / Ctrl+C | コピー |
| Cmd+X / Ctrl+X | カット |
| Cmd+V / Ctrl+V | ペースト |
| Shift+←/→ | 選択範囲を拡張 |
| ←/→（選択中） | 選択解除して端にカーソル移動 |
| Enter | 改行（有効時）+ onEnter コールバック |
| Backspace/Delete | 選択中は範囲削除、それ以外は 1 文字削除 |

### 自動折り返し (Word Wrap)

`setMaxWidth()` を呼ぶと、確定テキストと変換中テキストの両方が指定幅で自動改行される。

- 確定テキスト: データモデル上でソフト改行として `lines_` に格納。`getString()` にはソフト改行は含まれない
- 変換中テキスト (markedText): 描画時にフォントメトリクスで視覚的に折り返し
- 左右矢印キー: ソフト改行の境界をシームレスに跨ぐ
- Backspace / Delete: ソフト改行境界で正しく文字を削除

```cpp
ime_.setMaxWidth(300);  // 300px で折り返し
```

### UTF 変換ユーティリティ（static）

```cpp
string  utf8  = tcxIME::UTF32toUTF8(u32str);
u32string u32 = tcxIME::UTF8toUTF32(utf8str);
```

---

## ファイル構成

```
tcxIME/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── tcxIMEBase.h       # テキストエンジン基底クラス（プラットフォーム非依存）
│   ├── tcxIME.h           # メインクラス + 全ヘッダインクルード
│   ├── tcxIME.cpp         # コアロジック、キー操作、折り返し
│   ├── tcxTextField.h     # tcxIME::TextField（RectNode ラッパー）
│   ├── tcxFloatField.h    # tcxIME::FloatingPointField_<T>
│   ├── tcxIntField.h      # tcxIME::IntegerField_<T>
│   ├── tcxIMEView.h       # NSTextInputClient ビュー宣言 (macOS)
│   ├── tcxIMEView.mm      # NSTextInputClient 実装 (macOS)
│   ├── tcxIME_mac.mm      # macOS 入力ソース監視、IME ビュー管理
│   ├── tcxIME_win.cpp     # Windows IMM32 API による IME 実装
│   └── tcxIME_stub.cpp    # Linux 用スタブ
└── example-ime/           # サンプルアプリ
    └── src/
        ├── main.cpp
        ├── tcApp.h
        └── tcApp.cpp
```

## プラットフォーム対応状況

| Platform | Status |
|----------|--------|
| macOS | Full support (NSTextInputClient + Carbon) |
| Windows | Full support (IMM32 API + WndProc subclass) |
| Linux | Compiles (stub) — IME not yet implemented |
| Web (Emscripten) | Compiles (stub) — IME not yet implemented |

## 注意事項

- `draw()` の前に必ず `setFont()` を呼ぶこと
- ヘッダ内で `using namespace tc;` は使用禁止（macOS の `Rect` 型と衝突する）。ヘッダでは `tc::Vec2` 等を明示的に修飾する
- ウィンドウあたり 1 つの IME ビューを共有（参照カウント管理）
- `#include <tcxIME.h>` 1 つで全クラス（TextField, FloatField, IntField 等）が使える

## License

Same as TrussC.
