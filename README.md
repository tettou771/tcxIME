# tcxIME

Native OS IME (Input Method Editor) addon for [TrussC](https://github.com/toru-music/trussc).
Enables Japanese / CJK text input with kanji conversion, inline marked-text rendering, and automatic word wrapping.

> **Platform support:** macOS only (full IME). Windows / Linux compile with stubs but IME is not yet functional.

---

## 概要

tcxIME は TrussC 用の IME アドオンで、macOS のネイティブ日本語入力（かな漢字変換）をアプリに組み込める。

- ひらがな入力 → 漢字変換 → 確定 の一連のフローをサポート
- 変換候補ウィンドウの表示
- マルチライン対応（Enter で改行、上下矢印で行移動）
- `maxWidth` 指定による自動折り返し（確定テキスト・変換中テキスト両対応）
- テキスト選択（Cmd+A）、ペースト（Cmd+V）
- 英数 / かな モードの OS 同期

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

## 基本的な使い方

```cpp
#include <TrussC.h>
#include <tcxIME.h>
using namespace std;
using namespace tc;

class tcApp : public App {
    tcxIME ime_;

    void setup() override {
        ime_.setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 24);
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

## API

### tcxIME（メインクラス）

| メソッド | 説明 |
|---------|------|
| `setFont(path, size)` | フォントをファイルから読み込み |
| `setFont(Font*)` | 既存の Font を共有 |
| `setMaxWidth(float)` | 自動折り返し幅を設定（0 で無効） |
| `enable()` | IME 入力を有効化 |
| `disable()` | IME 入力を無効化 |
| `clear()` | テキストを全消去 |
| `draw(x, y)` | テキスト・カーソル・変換候補を描画 |
| `getString()` | 確定テキストを UTF-8 で取得 |
| `setString(str)` | UTF-8 文字列をセット |
| `isEnabled()` | 有効状態を返す |
| `isJapaneseMode()` | かな/変換中なら true |

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

## ファイル構成

```
tcxIME/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── tcxIME.h          # クラス定義 (tcxIMEBase, tcxIME)
│   ├── tcxIME.cpp         # コアロジック、キー操作、折り返し
│   ├── tcxIMEView.h       # NSTextInputClient ビュー宣言 (macOS)
│   ├── tcxIMEView.mm      # NSTextInputClient 実装 (macOS)
│   ├── tcxIME_mac.mm      # macOS 入力ソース監視、IME ビュー管理
│   └── tcxIME_stub.cpp    # Windows/Linux 用スタブ
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
| Windows | Compiles (stub) — IME not yet implemented |
| Linux | Compiles (stub) — IME not yet implemented |
| Web (Emscripten) | Compiles (stub) — IME not yet implemented |

## 注意事項

- `draw()` の前に必ず `setFont()` を呼ぶこと
- ヘッダ内で `using namespace tc;` は使用禁止（macOS の `Rect` 型と衝突する）。ヘッダでは `tc::Vec2` 等を明示的に修飾する
- ウィンドウあたり 1 つの IME ビューを共有（参照カウント管理）

## License

Same as TrussC.
