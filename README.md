# Zenoh ESP32 Setup Guide

## Laptop Setup

### 1. Download Zenoh

- **Windows:** Download from [this link](https://www.eclipse.org/downloads/download.php?file=/zenoh/zenoh/latest/zenoh-1.7.2-x86_64-pc-windows-gnu-standalone.zip)
- **Other OS:** Select the appropriate version from the [Zenoh downloads page](https://download.eclipse.org/zenoh/zenoh/latest/)

### 2. Start the Zenoh Router

1. Extract the downloaded ZIP file and navigate into the extracted folder
2. Get your IPv4 address:
   ```
   ipconfig
   ```
3. Start the router:
   ```
   .\zenohd.exe -l tcp/0.0.0.0:7447
   ```

### 3. Set Up the Python Subscriber

1. Copy Henry's GitHub repo into the extracted ZIP folder
2. Navigate to `z_sub.py` and update the code to match [this repo](https://github.com/Samepia1/Samepia1-laptop-esp-zenoh_pico-BNO086-imu-communication) <!-- replace # with actual link -->
3. The file location is /ZenohTest/z_sub.py
4. In the terminal, navigate to that directory and run:
   ```bash
   uv init
   uv venv
   .\.venv\Scripts\activate     # Windows
   # venv/scripts/activate      # Alternative
   uv sync
   uv run z_sub.py
   ```

---

## PlatformIO Environment Setup

### 1. Create the Project Folder

```bash
mkdir -p ~/project/zenoh-esp32
cd ~/project/zenoh-esp32
```

> For more on PlatformIO, check out [this repo](#). <!-- replace # with actual link -->

### 2. Initialize the Project

```bash
pio project init -b esp32dev
```

### 3. Add the Source Code

Copy and paste everything from the GitHub repo into the folder, or clone it directly. Make sure all dependencies are present (e.g., the Zenoh library).

### 4. Build & Flash

| Command | Description |
|---|---|
| `pio run` | Build the project |
| `pio run -t upload` | Build and flash to the ESP32 |
| `pio device monitor` | View serial output / print statements |

---

## Pinout table connnections

| Sensor wire color | Pin on the ESP32 board |
|---|---|
| White (+ voltage) | 3.3V power output pin |
| Black (ground) | Ground pin |
| Grey (signal) | Rx2 Pin (near D4 and D5 pins) |

---

## Troubleshooting

### USB Connection Issues

If you're having trouble connecting to the ESP32 via USB, run:

```bash
sudo chmod 666 /dev/ttyUSB[usb_number_which_gives_error]
```

Replace `[usb_number_which_gives_error]` with the actual device number (e.g., `ttyUSB0`).
