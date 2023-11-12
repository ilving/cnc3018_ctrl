# Proxy controller for CNC3018
### Fetures:
- [x] Wired proxy from Candle to cnc3018
- [x] Z-axis rotary encoder + x1\x10 move-per-rev flag

### ToDo:
- [ ] CNC3018 response parser (state, MPos, probe, override) 
- [ ] Case for encores, pcb, ...
- [ ] Screen (display probe, coords(?), overrides, state, errors)
- [ ] XY and Z feed override read from variable resiztor and CNC
- [ ] Rotary encoders for X and Y
- [ ] x1 \ x10 (1 or 10mm per encoder's turnover) flag for X and Y
- [ ] Buttons (or 4x4 keyboard) for homing\reset\G54..59\etc
- [ ] (probably) WiFi over additional ESP8266
- [ ] (probably) Feederate speed variable resistors for X+Y and Z axis