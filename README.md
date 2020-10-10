# BF-018A
JJY Simulator for M5Atom Lite/Matrix

M5Atom Lite, M5Atom Matrix で動作する標準電波（JJY）シミュレータ

### 1. 概要
　M5Atom で電波時計のための JJY もどきを生成します。JJY が届かないところにある電波時計の時刻合わせができます。Wifi 経由 NTP で時刻を取得し、GPIO から JJY 信号を出力します。
 
解説記事: https://qiita.com/BotanicFields/items/a78c80f947388caf0d36

### 2. アンテナの準備
　送信にはアンテナが必要です。GPIO22 と GND 間に 1kΩ 程度の抵抗を途中に挟んで 1m 程度の電線を接続して実験できます。電線を電波時計の至近距離に這わせると、電波時計が電線からの磁界を受信してくれます。

### 3. 動作
- 電源投入またはリセット後、まず Wifi 接続の動作に入ります。
- Wifi 接続が完了後、NTP で日時を取得し、標準信号の送出を開始します。 
- JJY 信号送出中を内蔵 LED でモニターできます。

### 4. Wifi 接続
　WiFiManager を使用しています。使い方は、WiFiManager の説明を参照ください。

https://github.com/tzapu/WiFiManager

### 5. LED
　ボタン で LED によるモニターをオン・オフできます。
 
  青色: 40kHz 信号の送出中を示します。
  赤色: Wifi 接続が切れると、点灯します。
  緑色: NTP による時刻取得に失敗すると、点灯します。
 
