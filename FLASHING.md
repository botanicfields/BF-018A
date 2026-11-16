# Flashing guide
How to install the software and upload (flash) the JJY simulator firmware to an ATOMS3 Lite or an ATOM Lite/Matrix from Windows, macOS or Linux. For what the firmware does and how to use it, see the [README](./README.md).

You can build and upload with either of two tools. Both use the same board package and libraries, so pick whichever you prefer:
- **Arduino IDE 2**, a graphical editor: [section 4](#4-option-a-arduino-ide-2)
- **arduino-cli**, a command-line tool: [section 5](#5-option-b-arduino-cli)

## Contents
1. [Choose a revision and the matching software versions](#1-choose-a-revision-and-the-matching-software-versions)
2. [What you need](#2-what-you-need)
3. [Prepare your computer](#3-prepare-your-computer): [Windows](#31-windows), [macOS](#32-macos), [Linux](#33-linux)
4. [Option A: Arduino IDE 2](#4-option-a-arduino-ide-2)
5. [Option B: arduino-cli](#5-option-b-arduino-cli)
6. [Settings to check before uploading](#6-settings-to-check-before-uploading)
7. [After uploading: the first start](#7-after-uploading-the-first-start)
8. [Switching to another set of versions](#8-switching-to-another-set-of-versions)
9. [Troubleshooting](#9-troubleshooting)

### Quick reference

| Item | Value |
|:--|:--|
| Board manager URL | `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json` |
| Board package | M5Stack by M5Stack official: 3.3.10 or 3.2.3 (Rev.4, Rev.5), or 2.1.4 (Rev.3, Rev.5 on the ATOM Lite/Matrix) |
| Board for the ATOMS3 Lite | M5AtomS3 (`m5stack:esp32:m5stack_atoms3`) |
| Board for the ATOM Lite/Matrix | M5Atom (`m5stack:esp32:m5stack_atom`) |
| Libraries with board package 3.3.10 | M5Unified 0.2.28, M5GFX 0.2.35, FastLED 3.10.8, WiFiManager 2.0.17 |
| Libraries with board package 3.2.3 | M5Unified 0.2.10, M5GFX 0.2.16, FastLED 3.10.3, WiFiManager 2.0.17 |
| Libraries with board package 2.1.4 | M5Unified 0.2.5, M5GFX 0.2.6, FastLED 3.10.3, WiFiManager 2.0.17 |
| Serial monitor | 115200 baud |

## 1. Choose a revision and the matching software versions
Each revision needs a particular version of the M5Stack board package, and the library versions that go with it. The versions below have been tested together; install all the versions of one set:

| Set | M5Stack board package | M5Unified | M5GFX | FastLED | WiFiManager |
|:--|:-:|:-:|:-:|:-:|:-:|
| 3.3.10 (the newest) | 3.3.10 | 0.2.28 | 0.2.35 | 3.10.8 | 2.0.17 |
| 3.2.3 | 3.2.3 | 0.2.10 | 0.2.16 | 3.10.3 | 2.0.17 |
| 2.1.4 | 2.1.4 | 0.2.5 | 0.2.6 | 3.10.3 | 2.0.17 |

| Revision (folder) | Boards | Sets | Status in the README |
|:--|:--|:--|:--|
| Rev.5 (`BF-018ARev5`) | ATOMS3 Lite, ATOM Lite/Matrix | 3.3.10 or 3.2.3; on the ATOM Lite/Matrix also 2.1.4 | Compile-tested; not yet tested on hardware |
| Rev.4 (`BF-018ARev4`) | ATOMS3 Lite | 3.3.10 or 3.2.3 | Confirmed working 2025-08-15 with the 3.2.3 set; the 3.3.10 set and later fixes compile-tested only |
| Rev.3 (`BF-018ARev3`) | ATOM Lite/Matrix, ATOMS3 Lite | 2.1.4 | Confirmed working 2025-08-15; later fixes compile-tested only |

The 3.3.10 set holds the newest versions available when this guide was written (October 2026). Boards Manager and Library Manager offer the newest versions by default, so if they offer something newer than these, it has not been tested with this project yet. Choose the 3.2.3 set instead if you want the versions Rev.4 was confirmed working with on hardware.

- Rev.4 only compiles with board package 3.x and Rev.3 only with 2.x: each uses LEDC (PWM) functions that exist in just one of them. Rev.5 compiles with either.
- Rev.4 on the ATOM Lite/Matrix: the README recommends Rev.3 instead, because the LED fix in Rev.4 has not been tested on hardware.
- Firmware built with board package 2.x stops keeping correct time in January 2038. Board package 3.x does not have this limit.
- Rev.1 (`BF-018A`) and Rev.2 (`BF-018ARev2`) are older versions; use Rev.3 or later.
- The standard ATOMS3 (the model with a screen) and the ATOMS3R are not supported.

If you are not sure which to choose: Rev.5 adds the web UI ([README section 9](./README.md#9-web-ui-rev5)) and supports all three boards, but has not yet been tested on hardware. Without the web UI, use Rev.4 on the ATOMS3 Lite and Rev.3 on the ATOM Lite/Matrix.

The README's tests used Arduino IDE 2.3.6 and arduino-cli 1.5.2-rc.1. The steps below are the same for newer 2.x releases of the IDE and 1.x releases of arduino-cli.

## 2. What you need
- The board: an ATOMS3 Lite, ATOM Lite or ATOM Matrix.
- A USB-C cable that carries data. Some cables only carry power; with one of those, the computer never sees the board.
- A computer running Windows 10 or 11, macOS, or Linux, with an internet connection for the downloads. The board package is a large download: about 1.7 GB for 3.3.10, 1.5 GB for 3.2.3 and 0.8 GB for 2.1.4, and it takes more space once unpacked.
- The antenna ([README section 3](./README.md#3-hardware)) and an RTC unit for Rev.3/Rev.4 ([README section 8(2)](./README.md#2-using-an-rtc-rev3rev4)) are not needed for flashing.

What each operating system needs besides the IDE or arduino-cli (details in [section 3](#3-prepare-your-computer)):

| | Windows 10/11 | macOS | Linux |
|:--|:--|:--|:--|
| USB driver for the ATOM Lite/Matrix | Usually installed automatically; otherwise the FTDI VCP driver | Built in; otherwise the FTDI VCP driver | Built in |
| USB driver for the ATOMS3 Lite | Installed automatically | Built in | Built in |
| Python 3 | Not needed | Xcode Command Line Tools | Usually installed already; with board package 2.1.4, also pyserial |
| Other | | Rosetta 2, on Apple silicon with board package 2.1.4 only | Permission to use serial ports; FUSE 2 for the Arduino IDE AppImage |

### Get the code
Download this repository: on its [GitHub page](https://github.com/tunlezah/JJYSim), choose **Code** > **Download ZIP** and extract the ZIP file, or clone it with Git:

```sh
git clone https://github.com/tunlezah/JJYSim.git
```

Each revision is a sketch folder (`BF-018ARev3`, `BF-018ARev4`, `BF-018ARev5`, ...) that holds an `.ino` file with the same name, and the files that go with it.

## 3. Prepare your computer

### 3.1 Windows
1. **ATOM Lite/Matrix:** plug the board in and open Device Manager. With its driver installed, the board appears under **Ports (COM & LPT)** as **USB Serial Port (COM*n*)**. Windows usually installs this FTDI driver by itself through Windows Update. If the board appears instead under **Other devices**, as **M5Stack** or **USB Serial** with a warning sign, install the FTDI VCP driver:
   1. Download the driver for your version of Windows from the [ATOM Lite page](https://docs.m5stack.com/en/core/Atom-Lite) on M5Stack's site, or from [FTDI](https://ftdichip.com/drivers/vcp-drivers/), and extract it.
   2. In Device Manager, right-click the device, choose **Update driver** > **Browse my computer for drivers**, and select the extracted folder.
   3. M5Stack notes that on some systems this has to be done twice before the COM port appears.
2. **ATOMS3 Lite:** nothing to install. Windows 10 and 11 set up the ESP32-S3's built-in USB port by themselves (the first time, this can need an internet connection). The board appears under **Ports (COM & LPT)** as **USB Serial Device (COM*n*)**. A second device, **USB JTAG/serial debug unit**, is only used for debugging and can be ignored.
3. Python is not needed: on Windows, the build tools in the board package are stand-alone programs.

### 3.2 macOS
1. **Install the Xcode Command Line Tools.** The board package runs `python3` during every build, and on macOS `python3` comes with these tools. In Terminal:

   ```sh
   xcode-select --install
   ```

   If they are already installed, the command says so. `python3 --version` should then print a version number.
2. **Apple silicon (M1 or later) with board package 2.1.4 only** (Rev.3, or Rev.5 built with 2.x): install Rosetta 2, because the esptool in that package only exists for Intel Macs:

   ```sh
   softwareupdate --install-rosetta --agree-to-license
   ```

   Board packages 3.3.10 and 3.2.3 run natively and don't need it.
3. **USB drivers:**
   - ATOMS3 Lite: none needed. The board appears as `/dev/cu.usbmodem...`.
   - ATOM Lite/Matrix: its USB serial interface is FTDI-compatible, and macOS's built-in driver normally handles it: the board appears as `/dev/cu.usbserial-...`. If it doesn't, install the FTDI VCP driver for macOS from the [ATOM Lite page](https://docs.m5stack.com/en/core/Atom-Lite) or [FTDI](https://ftdichip.com/drivers/vcp-drivers/), and allow it in **System Settings** > **Privacy & Security** (**System Preferences** > **Security & Privacy** on older macOS) if macOS asks.

   To see which port is the board, run `ls /dev/cu.*` before and after plugging it in.

### 3.3 Linux
1. **Permission to use serial ports.** Add your user to the group that owns them, then log out and back in (or restart):
   - Debian, Ubuntu, Linux Mint, Fedora, openSUSE: `sudo usermod -a -G dialout $USER`
   - Arch Linux, Manjaro: `sudo usermod -a -G uucp $USER`

   After logging in again, `groups` should list the group.
2. **Python 3.** The board package runs `python3` during every build; most distributions include it. With board package **2.1.4** (Rev.3, or Rev.5 built with 2.x), esptool also runs under Python and needs pyserial, both to build and to upload:
   - Debian, Ubuntu, Linux Mint: `sudo apt install python3 python3-serial`
   - Fedora: `sudo dnf install python3 python3-pyserial`
   - Arch Linux, Manjaro: `sudo pacman -S python python-pyserial`
   - openSUSE: `sudo zypper install python3 python3-pyserial`
3. **USB drivers** are built into the kernel. The ATOM Lite/Matrix appears as `/dev/ttyUSB0` and the ATOMS3 Lite as `/dev/ttyACM0` (the number is higher if similar devices are already connected). `ls /dev/ttyUSB* /dev/ttyACM*` lists them.
4. **Arduino IDE AppImage only:** it needs FUSE 2. arduino-cli doesn't.
   - Ubuntu 22.04, Debian 12: `sudo apt install libfuse2`
   - Ubuntu 24.04 or later, Debian 13: `sudo apt install libfuse2t64`
   - Fedora: `sudo dnf install fuse-libs`
   - Arch Linux, Manjaro: `sudo pacman -S fuse2`

## 4. Option A: Arduino IDE 2

### 4.1 Install the IDE
Download Arduino IDE 2 from [arduino.cc/en/software](https://www.arduino.cc/en/software).
- **Windows:** run the installer (`.exe`). An MSI installer and a ZIP file are also offered.
- **macOS:** open the `.dmg` (there are builds for Apple silicon and for Intel) and drag **Arduino IDE** to **Applications**.
- **Linux:** download the AppImage, then, in the folder you downloaded it to, make it executable and start it:

  ```sh
  chmod +x arduino-ide_*_Linux_64bit.AppImage
  ./arduino-ide_*_Linux_64bit.AppImage
  ```

  It needs FUSE 2 ([section 3.3](#33-linux)). On Ubuntu 24.04 and later it may stop with a message about the "SUID sandbox helper binary": Ubuntu restricts the sandbox that the IDE's Chromium-based window uses. The usual workaround is to start it with `--no-sandbox`, which turns that sandbox off for the IDE. If you'd rather not do that, use arduino-cli ([section 5](#5-option-b-arduino-cli)) instead.

### 4.2 Add the M5Stack board manager URL
1. Open **File** > **Preferences...** (on macOS: **Arduino IDE** > **Settings...**).
2. In **Additional boards manager URLs**, enter:

   ```
   https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
   ```

   If the field already holds URLs, click the button next to it and add this one on a new line. M5Stack also has a mirror for users in China: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index_cn.json`.
3. Click **OK**.

### 4.3 Install the board package
1. Open **Tools** > **Board** > **Boards Manager...** (or the boards icon in the left sidebar).
2. Search for `M5Stack` and find **M5Stack** by **M5Stack official**. (**esp32** by **Espressif Systems** is a different package, and not the one the README was tested with.)
3. In its version list, choose the version of your set: **3.3.10**, **3.2.3** or **2.1.4** (see [section 1](#1-choose-a-revision-and-the-matching-software-versions)), then click **Install**. The list starts at the newest version; if that is newer than 3.3.10, it has not been tested with this project.
4. Wait for the download and installation to finish. It can take a while (see [section 2](#2-what-you-need)).

### 4.4 Install the libraries
Open **Tools** > **Manage Libraries...** (or the books icon in the left sidebar). For each library below: search for it, choose the version in its version list, and click **Install**.

| Search for | Entry to install | 3.3.10 set | 3.2.3 set | 2.1.4 set |
|:--|:--|:-:|:-:|:-:|
| `M5Unified` | M5Unified by M5Stack | 0.2.28 | 0.2.10 | 0.2.5 |
| `M5GFX` | M5GFX by M5Stack | 0.2.35 | 0.2.16 | 0.2.6 |
| `FastLED` | FastLED by Daniel Garcia | 3.10.8 | 3.10.3 | 3.10.3 |
| `WiFiManager` | WiFiManager by tzapu | 2.0.17 | 2.0.17 | 2.0.17 |

When you install M5Unified, the IDE asks whether to install its dependency, M5GFX. Click **Install without dependencies**, and install M5GFX yourself at the version in the table. **Install All** would install the newest M5GFX instead, replacing a version you have already installed. If that happens, install M5GFX again at the version in the table.

Several libraries have "WiFiManager" in their names; the one to install is **WiFiManager** by **tzapu**.

To check the result, set **Type** to **Installed** at the top of Library Manager.

The IDE may tell you that updates are available for some of your boards and libraries. Don't install them: they replace the tested versions.

### 4.5 Open the sketch
Choose **File** > **Open...** and open the `.ino` file in the folder of the revision you chose, for example `BF-018ARev5/BF-018ARev5.ino`. The other files in the folder open as tabs. Don't rename the folder: the Arduino tools need it to have the same name as the `.ino` file.

Before uploading, check the settings at the top of the `.ino` file: see [section 6](#6-settings-to-check-before-uploading).

### 4.6 Select the board and the port
1. Connect the board with the USB cable.
2. Choose **Tools** > **Board** > **M5Stack** > **M5AtomS3** for the ATOMS3 Lite, or **M5Atom** for the ATOM Lite/Matrix. Don't choose M5AtomS3R.
3. Check these settings in the **Tools** menu. They are the defaults when you choose the board, so you only need to change them if they were changed before.

   ATOMS3 Lite (M5AtomS3):

   | Setting | Value |
   |:--|:--|
   | USB CDC On Boot | Enabled |
   | USB Mode | Hardware CDC and JTAG |
   | Upload Mode | UART0 / Hardware CDC |
   | Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
   | Upload Speed | 921600 |
   | Erase All Flash Before Sketch Upload | Disabled |

   The firmware relies on the two USB settings: with them, its serial output goes to the USB port, and the code that keeps serial output from stalling the time signal when a connected PC stops reading is compiled in.

   ATOM Lite/Matrix (M5Atom):

   | Setting | Value |
   |:--|:--|
   | Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
   | Upload Speed | 1500000 (choose 115200 if uploads fail) |
   | Erase All Flash Before Sketch Upload | Disabled |

4. Choose the board's port in **Tools** > **Port**:
   - Windows: the board's `COM` port (Device Manager shows which one, see [section 3.1](#31-windows)).
   - macOS: `/dev/cu.usbmodem...` for the ATOMS3 Lite, `/dev/cu.usbserial-...` for the ATOM Lite/Matrix.
   - Linux: `/dev/ttyACM0` for the ATOMS3 Lite, `/dev/ttyUSB0` for the ATOM Lite/Matrix.

   If you aren't sure which port is the board, unplug it, look at the list, then plug it in again and see which port appears.

### 4.7 Upload
Click the **Upload** button (the right arrow), or choose **Sketch** > **Upload**. The IDE compiles the sketch (the first build takes a few minutes), then uploads it. The output ends with a line such as `Hard resetting via RTS pin...`, and the board restarts with the new firmware.

If the upload can't connect:
- **ATOMS3 Lite:** put it into download mode by hand. Hold the reset button down for about 2 seconds, until the green LED inside lights up, then release it; the LED goes off. Select the port again (its name can change), and upload. When the upload has finished, press the reset button once to start the firmware.
- **ATOM Lite/Matrix:** set **Tools** > **Upload Speed** to **115200** and upload again.

More fixes are in [section 9](#9-troubleshooting).

### 4.8 Watch the serial monitor
Open **Tools** > **Serial Monitor** and set the speed to **115200 baud**. You can then follow what the device is doing ([README section 7](./README.md#7-serial-monitor)). Press the reset button to see the messages from the start. Rev.5 also shows this log on its web page.

## 5. Option B: arduino-cli

### 5.1 Install arduino-cli
- **macOS**, with [Homebrew](https://brew.sh/):

  ```sh
  brew install arduino-cli
  ```

- **Linux**, and macOS without Homebrew: the official install script puts the program in the folder given by `BINDIR`, which must already exist:

  ```sh
  mkdir -p ~/.local/bin
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
  ```

  If `~/.local/bin` is not on your `PATH`, the script says so: add it, or choose a folder that is. You can also download an archive and extract `arduino-cli` from it yourself: see the [installation page](https://arduino.github.io/arduino-cli/latest/installation/).
- **Windows:** download the [MSI installer](https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.msi) or the [ZIP file](https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip). With the ZIP file, put `arduino-cli.exe` in a folder on your `PATH`.

Then, in a new terminal (PowerShell on Windows), check that it runs:

```sh
arduino-cli version
```

### 5.2 Install the board package and the libraries
Run these once. The commands are the same on Windows, macOS and Linux.

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli core update-index
arduino-cli lib update-index
```

`config init` creates the configuration file. If you already have one, it says so; carry on with the next command.

Then install the board package and the libraries of the set you chose ([section 1](#1-choose-a-revision-and-the-matching-software-versions)):

```sh
# 3.3.10 set (the newest): Rev.4, Rev.5
arduino-cli core install m5stack:esp32@3.3.10
arduino-cli lib install --no-deps M5Unified@0.2.28 M5GFX@0.2.35 FastLED@3.10.8 WiFiManager@2.0.17
```

```sh
# 3.2.3 set: Rev.4, Rev.5
arduino-cli core install m5stack:esp32@3.2.3
arduino-cli lib install --no-deps M5Unified@0.2.10 M5GFX@0.2.16 FastLED@3.10.3 WiFiManager@2.0.17
```

```sh
# 2.1.4 set: Rev.3, Rev.5 on the ATOM Lite/Matrix
arduino-cli core install m5stack:esp32@2.1.4
arduino-cli lib install --no-deps M5Unified@0.2.5 M5GFX@0.2.6 FastLED@3.10.3 WiFiManager@2.0.17
```

`--no-deps` stops arduino-cli from installing the newest M5GFX as a dependency of M5Unified; the command installs the right M5GFX itself. `arduino-cli core list` and `arduino-cli lib list` show what is installed.

### 5.3 Compile and upload
1. Connect the board and find its port:

   ```sh
   arduino-cli board list
   ```

   The port is in the first column: for example `COM5` on Windows, `/dev/cu.usbmodem1101` on macOS or `/dev/ttyACM0` on Linux ([section 4.6](#46-select-the-board-and-the-port) lists the names for each board).
2. In the repository folder (the one that holds the revision folders), compile and upload. Replace `PORT` with the port, and `BF-018ARev5` with the folder of the revision you chose:

   ```sh
   # ATOMS3 Lite
   arduino-cli compile --upload -p PORT -b m5stack:esp32:m5stack_atoms3 BF-018ARev5
   ```

   ```sh
   # ATOM Lite/Matrix
   arduino-cli compile --upload -p PORT -b m5stack:esp32:m5stack_atom BF-018ARev5
   ```

   Without `--upload -p PORT`, the command only compiles: a quick way to check the setup without a board.

The board settings are the defaults listed in [section 4.6](#46-select-the-board-and-the-port). To change one, add it to the board name after a colon. For example, to upload to an ATOM Lite/Matrix at 115200 baud:

```sh
arduino-cli compile --upload -p PORT -b m5stack:esp32:m5stack_atom:UploadSpeed=115200 BF-018ARev5
```

If an upload to an ATOMS3 Lite can't connect, use download mode as described in [section 4.7](#47-upload).

### 5.4 Watch the serial monitor

```sh
arduino-cli monitor -p PORT --config baudrate=115200
```

Press Ctrl+C to quit.

## 6. Settings to check before uploading
These settings are in the `.ino` file of each revision. Change them before uploading if the defaults don't suit you.

**Rev.3 and Rev.4** (near the top of `BF-018ARev3.ino` or `BF-018ARev4.ino`):

| Setting | Default | Meaning |
|:--|:--|:--|
| `jjy_frequency` | `40000` | Carrier frequency: 40 kHz or 60 kHz ([README section 8(4)](./README.md#4-using-40-khz)) |
| `time_zone` | `"JST-9"` | Time zone of the time sent, as a POSIX TZ string ([README section 9](./README.md#9-web-ui-rev5) explains how to choose it) |
| `ntp_server` | `"pool.ntp.org"` | NTP server |

**Rev.5** (at the top of `BF-018ARev5.ino`):

| Setting | Default | Meaning |
|:--|:--|:--|
| `settings` | `40000`, `"JST-9"`, `"pool.ntp.org"` | Defaults for the carrier frequency, time zone and NTP server. These can also be changed later on the web page. |
| `web_password` | `""` (empty) | Password for changing settings on the web page. While it is empty, the page is read-only ([README section 9(1)](./README.md#1-password)). |
| `web_user` | `"admin"` | User name for the web page |
| `web_hostname` | `"jjy-sim"` | Host name: the page is at `http://jjy-sim.local/` |

The password is stored in the firmware as plain text. If you keep your copy of the code in Git, take care not to commit the password.

In Rev.5, settings saved from the web page are stored in the board's flash memory and take precedence over the defaults in the sketch, also after you upload new firmware. A normal upload keeps them, and also keeps the stored Wi-Fi network. To erase both, set **Tools** > **Erase All Flash Before Sketch Upload** to **Enabled** for one upload (with arduino-cli, add `:EraseFlash=all` to the board name), then set it back to **Disabled**.

## 7. After uploading: the first start
1. **Wi-Fi.** The board first tries the Wi-Fi network it used last time. The first time, or if that network can't be found, it opens a setup network of its own, and the LED lights green:
   1. On a phone or computer, join the open Wi-Fi network whose name starts with `ESP32_` (followed by the chip ID).
   2. Open `http://192.168.4.1` in a browser, choose your network and enter its password.

   The setup network stays open for 60 seconds. If it closes before you have finished, press the reset button to open it again. The boards only use 2.4 GHz Wi-Fi, so choose a 2.4 GHz network. More details are in [README section 5](./README.md#5-wi-fi-connection).
2. **Time.** Once connected, the board gets the time by NTP and starts sending the time signal. The LED blinks blue with the signal ([README section 8(5)](./README.md#5-led-indications) explains the colours), and the serial monitor shows the time code as it is sent.
3. **Web UI (Rev.5).** Open `http://jjy-sim.local/`, or the IP address printed in the serial monitor ([README section 9](./README.md#9-web-ui-rev5)).

To connect to a different Wi-Fi network later, hold the button while pressing reset until the LED lights green ([README section 8(3)](./README.md#3-erasing-the-stored-ssidkey)), or, with Rev.5, use **Forget Wi-Fi network** on the web page.

## 8. Switching to another set of versions
Only one version of the M5Stack board package can be installed at a time, and the libraries must match it. To build a revision that needs another set ([section 1](#1-choose-a-revision-and-the-matching-software-versions)), install that set; this replaces the installed versions.
- **Arduino IDE:** in Boards Manager, choose the set's version of M5Stack and click **Install**. Then, in Library Manager, install M5Unified, M5GFX and FastLED at the set's versions (click **Install without dependencies** for M5Unified). WiFiManager is the same in every set.
- **arduino-cli:** run the set's two commands from [section 5.2](#52-install-the-board-package-and-the-libraries).

## 9. Troubleshooting

**No port appears for the board**
- Try another USB cable: some only carry power. Also try another USB socket, without a hub in between.
- Windows, ATOM Lite/Matrix: install the FTDI driver ([section 3.1](#31-windows)).
- ATOMS3 Lite: put it into download mode ([section 4.7](#47-upload)); the port should then appear.
- Linux: run `sudo dmesg -w` and plug the board in. If the port appears and then disappears with a message that mentions `brltty`, the braille display service has taken it over (this happens on Ubuntu 22.04). Unless you use a braille display, remove it: `sudo apt remove brltty`.

**Linux: `Permission denied` on `/dev/ttyUSB0` or `/dev/ttyACM0`**
- Add yourself to the `dialout` (or `uucp`) group and log in again ([section 3.3](#33-linux)).

**`Failed to connect to ESP32` (or `ESP32-S3`), for example with `No serial data received` or `Timed out waiting for packet header`; or `Could not open` the port**
- Close anything else that has the port open, such as a serial monitor in another program.
- ATOM Lite/Matrix: lower the upload speed to 115200 ([section 4.6](#46-select-the-board-and-the-port), [section 5.3](#53-compile-and-upload)).
- ATOMS3 Lite: use download mode ([section 4.7](#47-upload)).

**`This chip is ESP32-S3 not ESP32. Wrong --chip argument?`, or the reverse**
- The wrong board is selected: M5AtomS3 is for the ATOMS3 Lite, M5Atom for the ATOM Lite/Matrix.

**Compile error: `'ledcAttach' was not declared in this scope`**
- Rev.4 needs board package 3.x: install the 3.3.10 or 3.2.3 set ([section 8](#8-switching-to-another-set-of-versions)).

**Compile error: `'ledcSetup' was not declared in this scope`, or the same for `ledcAttachPin`**
- Rev.3 needs board package 2.x: install the 2.1.4 set ([section 8](#8-switching-to-another-set-of-versions)), or use Rev.4 or Rev.5.

**Compile error: `M5Unified.h: No such file or directory` (or `FastLED.h`, `WiFiManager.h`)**
- A library is missing: install it ([section 4.4](#44-install-the-libraries) or [section 5.2](#52-install-the-board-package-and-the-libraries)).

**Compile errors inside M5Unified or M5GFX**
- The library versions don't match the board package. Install the versions in [section 1](#1-choose-a-revision-and-the-matching-software-versions) for the board package you have.

**macOS: `xcrun: error: invalid active developer path`, or `python3` not found**
- Install the Xcode Command Line Tools ([section 3.2](#32-macos)).

**macOS on Apple silicon: `bad CPU type in executable`**
- Board package 2.1.4 needs Rosetta 2 ([section 3.2](#32-macos)).

**Linux: `Pyserial is not installed for /usr/bin/python3`**
- Board package 2.1.4 needs pyserial ([section 3.3](#33-linux)).

**Linux: the Arduino IDE AppImage doesn't start**
- `dlopen(): error loading libfuse.so.2`: install FUSE 2 ([section 3.3](#33-linux)).
- A message about the SUID sandbox: see [section 4.1](#41-install-the-ide).

**The serial monitor shows nothing, or garbled text**
- Set the speed to 115200 baud, and press the reset button.
- ATOMS3 Lite: check that **USB CDC On Boot** is **Enabled** and **USB Mode** is **Hardware CDC and JTAG** ([section 4.6](#46-select-the-board-and-the-port)), then upload again.

**The upload worked, but the board doesn't start**
- After download mode was entered by hand, an ATOMS3 Lite stays in it until it is reset: press the reset button.

**The LED stays off on an ATOM Lite/Matrix with Rev.4**
- This is the Rev.4 LED problem the README describes. Use Rev.3 on these boards.

**The board menu shows two entries called M5Atom**
- **esp32** by **Espressif Systems** is installed as well. Choose the entry under **M5Stack**.
