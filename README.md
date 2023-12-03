# Proxy controller for CNC3018
### Fetures:
- [x] Wired proxy from Candle to cnc3018
- [x] Z-axis rotary encoder + x1\x10 move-per-rev flag

### ToDo:
- [x] CNC3018 response parser (state, MPos, probe, override) 
- [x] Z feed override read from variable resiztor and CNC
- [x] Screen (display probe, coords(?), overrides, state, errors)
- [x] Rotary encoders for X and Y
- [x] Feed override read from variable resiztor and CNC
- [ ] Case for encores, pcb, ... (added at 04.12.2023, but it's needed to think)
- [ ] x1 \ x10 (1 or 10mm per encoder's turnover) flag for X and Y
- [ ] Buttons (or 4x4 keyboard) for homing\reset\G54..59\etc (PCF8574N or something like this)
- [ ] (probably) WiFi over additional ESP8266
- [ ] (probably) Bluetooth
- [ ] (probably) Feederate speed variable resistors for X+Y and Z axis