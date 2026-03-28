# Cavemen's Best Friend - System Architecture

## Hardware Connections
* **Seeed SenseCAP Indicator**: Central hub (ESP32-S3).
* **Logitech Webcam**: Connected via USB Type-C using an OTG adapter, utilizing ESP-IDF's USB Host Video Class (UVC) driver to grab frames.
* **Grove Button**: Connected to the Grove ADC/GPIO port as the system trigger.
* **Audio/Display**: Built-in screen and speaker for UI colors, animations, and sound effects.

## System Flow & Gemini API specifics
1. **Trigger Phase**: The ESP32-S3 monitors the Grove Button.
2. **Capture Phase**: On press, the system grabs one JPEG frame from the connected UVC Web Camera.
3. **Classification & Persona Phase**:
   * We will send the JPEG over Wi-Fi to the pre-existing **Gemini Gem** configured with the dog persona.
   * **API Specifics**: The ESP32 will make an HTTP request (via REST API or intermediate webhook) to the Gem, passing the image. The Gem will return a JSON payload with the threat level, UI color, and the dog persona's voice line/reaction.
4. **Action Phase**:
   * Parse the JSON response on the ESP32.
   * Update the LVGL UI background to the provided color.
   * Play the appropriate audio file via I2S (intense barking, growling, or friendly voice).
   * Display the voice line and animations on the screen.

## Project Structure (ESP-IDF)
Create a new project directory under `examples/cavemen_dog`.
* `main/main.c`: Application entry point and state machine.
* `main/hardware_init.c`: Wi-Fi, Button, Audio, and Display initialization.
* `main/usb_camera.c`: ESP-IDF USB UVC Host implementation for Logitech webcam.
* `main/gemini_client.c`: ESP-HTTP-Client implementation for sending multipart/form-data or Base64 JSON and receiving response.
* `main/ui.c`: LVGL screens for Idle, Loading, Threat Level.