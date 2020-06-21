# cloudlight

## Project Wiring

### Arduino MEGA PIN -> Wemos D1 Mini Pin
  * 5V -> 5V
  * GND -> GND
  * SDA 20 -> D2
  * SCL 21 -> D1

### Arduino MEGA PIN -> WS2811 LED Strip
  * GND -> GND
  * 2 -> DIN

### Arduino MEGA PIN -> MAX4466 Microphone
    * GND -> GND
    * A0 -> OUT

### Wemos D1 Mini Pin -> IR Receiver
  * 3V3 -> VIN (right pin)
  * D3 -> Signal (left pin)

### Three-way connections
  * MEGA GND -> Wemos GND -> IR Receiver GND (middle pin)
  * MEGA 3.3V -> MEGA AREF -> Microphone VCC

### 12V Power connections
  * Power in on MEGA
  * Power on LED Strip
  * (All others powered from MEGA to components)
