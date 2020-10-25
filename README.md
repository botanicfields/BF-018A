# BF-018A
JJY Simulator for M5Atom Lite/Matrix

M5Atom Lite, M5Atom Matrix で動作する標準電波（JJY）シミュレータ

### 1. 概要
　M5Atom で電波時計のための JJY もどきを生成します。JJY が届かないところにある電波時計の時刻合わせができます。Wifi 経由 NTP で時刻を取得し、GPIO から JJY 信号を出力します。
 
解説記事: https://qiita.com/BotanicFields/items/a78c80f947388caf0d36

### 2. ソフトウェア
- Arduino IDE for ESP32
- M5Atom ライブラリ .. 最新版を GitHub からダウンロードする必要があります。https://github.com/m5stack/M5Atom
- WifiManager (by Tzapu, Tablatronix) ライブラリ

### 3. アンテナの準備
　送信にはアンテナが必要です。GPIO22 と GND 間に 1kΩ 程度の抵抗を途中に挟んで 1m 程度の電線を接続して実験できます。電線を電波時計の至近距離に這わせると、電波時計が電線からの磁界を受信してくれます。

#### 3.1 プリント基板
　M5Atom に接続可能なプリント基板を製作しました。

回路図
https://github.com/botanicfields/BF-018A/blob/main/bf-018a_scm.pdf
プリント基板
![PCB_front](https://github.com/botanicfields/BF-018A/blob/main/BF-018A_front.JPG)
M５Atom Lite を搭載
![PCB_with_M5AtomLite](https://github.com/botanicfields/BF-018A/blob/main/BF-018A_M5AtomLite.JPG)
M５Atom Matrix を搭載
![PCB_with_M5AtomMatrix](https://github.com/botanicfields/BF-018A/blob/main/BF-018A_M5AtomMatrix.JPG)

### 4. 動作
- 電源投入またはリセット後、まず Wifi 接続の動作に入ります。
- Wifi 接続が完了後、NTP で日時を取得し、標準信号の送出を開始します。 
- JJY 信号送出中を内蔵 LED でモニターできます。
 
- 青色: 40kHz 信号の送出中を示します。
- 赤色: Wifi 接続が切れると点灯します。
- 緑色: NTP による時刻取得に失敗すると点灯します。

M5Atom Matrix では、0 番の LED のみを使用しています。0 番以外の LED が点滅することがありますが、原因は不明です。

### 5. Wifi 接続
　WiFiManager を使用しています。使い方は、WiFiManager の説明を参照ください。

https://github.com/tzapu/WiFiManager

### 6. ボタン
　ボタン で LED による JJY 信号送出モニターをオン・オフできます。
