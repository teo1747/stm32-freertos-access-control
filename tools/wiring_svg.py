#!/usr/bin/env python3
"""Draw the terminal's wiring as SVG: the NUCLEO's pins on the left, the
modules on the right, one colour per bus. Matches firmware/platform/board.h.

    tools/wiring_svg.py > docs/images/wiring.svg
"""
from xml.sax.saxutils import escape

W = 980
COLOURS = {
    "spi": "#e8590c", "i2c": "#1c7ed6", "pwm": "#9c36b5", "gpio": "#2f9e44",
    "keypad": "#495057", "power": "#c92a2a", "uart": "#0c8599",
}
LEGEND = [("spi", "SPI1"), ("i2c", "I2C1, 400 kHz"), ("pwm", "Timer PWM"), ("gpio", "GPIO"),
          ("keypad", "Keypad matrix"), ("uart", "USART1, planned")]

# (header pin, MCU pin, signal, bus, module, module pin)
PINS = [
    ("D3", "PB3", "SPI1_SCK", "spi", "rfid", "SCK"),
    ("D5", "PB4", "SPI1_MISO", "spi", "rfid", "MISO"),
    ("D4", "PB5", "SPI1_MOSI", "spi", "rfid", "MOSI"),
    ("D10", "PB6", "CS", "spi", "rfid", "SDA (NSS)"),
    ("D7", "PA8", "RST", "spi", "rfid", "RST"),
    ("D15", "PB8", "I2C1_SCL", "i2c", "i2c", "SCL"),
    ("D14", "PB9", "I2C1_SDA", "i2c", "i2c", "SDA"),
    ("D6", "PB10", "TIM2_CH3", "pwm", "servo", "signal"),
    ("D9", "PC7", "TIM3_CH2", "pwm", "buzzer", "+"),
    ("D11", "PA7", "LED green", "gpio", "leds", "green (330 R)"),
    ("D12", "PA6", "LED red", "gpio", "leds", "red (330 R)"),
    ("A4", "PC1", "exit button", "gpio", "switches", "exit button"),
    ("A5", "PC0", "door contact", "gpio", "switches", "reed contact"),
    ("A0-A3", "PA0,1,4 PB0", "rows", "keypad", "keypad", "R1-R4"),
    ("CN7 1-4", "PC10-12 PD2", "columns", "keypad", "keypad", "C1-C4"),
    ("D8, D2", "PA9, PA10", "USART1", "uart", "esp", "RX, TX"),
]

MODULES = {
    "rfid": ("MFRC522 RFID reader", "RC522 module, 3.3 V, SPI mode 0, 5.6 MHz"),
    "i2c": None,
    "servo": ("SG90 servo: the lock", "5 V from the NUCLEO's 5V pin"),
    "buzzer": ("Passive buzzer", "square wave at the tone's pitch"),
    "leds": ("Green and red LEDs", "to GND through 330 R"),
    "switches": ("Exit button, door reed", "to GND, internal pull-ups"),
    "keypad": ("4x4 membrane keypad", "rows open-drain, columns pulled up"),
    "esp": ("ESP32-C3 DevKitM-1", "Wi-Fi gateway (planned)"),
}
I2C_DEVICES = [("SSD1306 OLED 128x64", "0x3C"), ("DS3231 RTC + CR2032", "0x68"), ("24LC256 EEPROM, 32 KB", "0x50")]

order = ["rfid", "i2c", "servo", "buzzer", "leds", "switches", "keypad", "esp"]
wires = {k: sum(1 for p in PINS if p[4] == k) for k in order}
heights = {k: (len(I2C_DEVICES) * 64 + 22 if k == "i2c" else 52 if wires[k] == 1 else 50 + wires[k] * 20)
           for k in order}
H = 70 + sum(heights.values()) + 12 * len(order) + 60

out = []
add = out.append
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
    'font-family="ui-monospace, SFMono-Regular, Menlo, Consolas, monospace" font-size="13">')
add(f'<rect width="{W}" height="{H}" rx="14" fill="#ffffff"/>')
add(f'<text x="40" y="44" font-size="20" font-weight="bold" fill="#212529">Wiring: NUCLEO-F446RE access terminal</text>')

# The board
bx, by, bw = 40, 70, 300
row_h = 36
bh = 60 + row_h * len(PINS) + 50
add(f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="10" fill="#f1f3f5" stroke="#343a40" stroke-width="2"/>')
add(f'<text x="{bx + 16}" y="{by + 30}" font-size="16" font-weight="bold" fill="#212529">NUCLEO-F446RE</text>')
add(f'<text x="{bx + 16}" y="{by + 48}" fill="#495057">STM32F446RE, 180 MHz, FreeRTOS</text>')

module_y = {}
mx, mw = 600, 340
y = by
for key in order:
    module_y[key] = y
    y += heights[key] + 12

# I2C devices share a bus drawn as a vertical rail
i2c_rail_x = 560
pin_points = {}
for i, (hdr, mcu, sig, bus, mod, mpin) in enumerate(PINS):
    py = by + 80 + i * row_h
    c = COLOURS[bus]
    add(f'<circle cx="{bx + bw}" cy="{py}" r="5" fill="{c}"/>')
    add(f'<text x="{bx + 16}" y="{py + 4}" font-weight="bold" fill="#212529">{escape(hdr)}</text>')
    add(f'<text x="{bx + 90}" y="{py + 4}" fill="#495057">{escape(mcu)}</text>')
    add(f'<text x="{bx + bw - 12}" y="{py + 4}" text-anchor="end" fill="{c}">{escape(sig)}</text>')
    pin_points[i] = py

# Module boxes
for key in order:
    top = module_y[key]
    h = heights[key]
    if key == "i2c":
        rail_end = top + (len(I2C_DEVICES) - 1) * 64 + 34
        add(f'<line x1="{i2c_rail_x}" y1="{top + 20}" x2="{i2c_rail_x}" y2="{rail_end}" stroke="{COLOURS["i2c"]}" stroke-width="3"/>')
        add(f'<line x1="{i2c_rail_x + 10}" y1="{top + 20}" x2="{i2c_rail_x + 10}" y2="{rail_end}" stroke="{COLOURS["i2c"]}" stroke-width="3"/>')
        for j, (name, addr) in enumerate(I2C_DEVICES):
            dy = top + j * 64
            add(f'<rect x="{mx}" y="{dy}" width="{mw}" height="52" rx="8" fill="#e7f5ff" stroke="{COLOURS["i2c"]}" stroke-width="1.5"/>')
            add(f'<text x="{mx + 14}" y="{dy + 22}" font-weight="bold" fill="#212529">{escape(name)}</text>')
            add(f'<text x="{mx + 14}" y="{dy + 40}" fill="#495057">I2C address {addr}, 3.3 V</text>')
            add(f'<line x1="{i2c_rail_x}" y1="{dy + 26}" x2="{mx}" y2="{dy + 26}" stroke="{COLOURS["i2c"]}" stroke-width="1.5"/>')
            add(f'<line x1="{i2c_rail_x + 10}" y1="{dy + 34}" x2="{mx}" y2="{dy + 34}" stroke="{COLOURS["i2c"]}" stroke-width="1.5"/>')
        add(f'<text x="{mx}" y="{top + len(I2C_DEVICES) * 64 + 8}" fill="{COLOURS["i2c"]}">SCL and SDA: 4.7 k pull-ups to 3.3 V</text>')
        continue
    title, note = MODULES[key]
    fill = "#fff4e6" if key == "rfid" else "#f8f9fa"
    add(f'<rect x="{mx}" y="{top}" width="{mw}" height="{h}" rx="8" fill="{fill}" stroke="#343a40" stroke-width="1.5"/>')
    add(f'<text x="{mx + 14}" y="{top + 22}" font-weight="bold" fill="#212529">{escape(title)}</text>')
    add(f'<text x="{mx + 14}" y="{top + 40}" fill="#495057">{escape(note)}</text>')

# Wires: from each pin to its module, with the module pin's name
counts = {}
for i, (hdr, mcu, sig, bus, mod, mpin) in enumerate(PINS):
    c = COLOURS[bus]
    py = pin_points[i]
    if mod == "i2c":
        tx = i2c_rail_x + (10 if mpin == "SDA" else 0)
        ty = module_y["i2c"] + 20
        add(f'<path d="M{bx + bw} {py} H{tx - 30} Q{tx} {py} {tx} {max(ty, py)}" fill="none" stroke="{c}" stroke-width="2"/>')
        continue
    n = counts.get(mod, 0)
    counts[mod] = n + 1
    top = module_y[mod]
    ty = top + 58 + n * 20 if wires[mod] > 1 else top + 26
    dash = ' stroke-dasharray="6 5"' if bus == "uart" else ""
    add(f'<path d="M{bx + bw} {py} C{bx + bw + 120} {py} {mx - 120} {ty} {mx} {ty}" fill="none" stroke="{c}" stroke-width="2"{dash}/>')
    add(f'<circle cx="{mx}" cy="{ty}" r="4" fill="{c}"/>')
    if wires[mod] > 1:
        add(f'<text x="{mx + 14}" y="{ty + 4}" fill="{c}">{escape(mpin)}</text>')

# Power and the console
py = by + 80 + len(PINS) * row_h
add(f'<text x="{bx + 16}" y="{py + 4}" fill="{COLOURS["power"]}" font-weight="bold">3V3, 5V, GND to every module</text>')
add(f'<text x="{bx + 16}" y="{py + 24}" fill="#495057">Console: USART2, ST-LINK USB</text>')

# Legend
lx, ly = 40, H - 24
for k, (bus, label) in enumerate(LEGEND):
    x = lx + k * 150
    add(f'<rect x="{x}" y="{ly - 10}" width="22" height="6" rx="3" fill="{COLOURS[bus]}"/>')
    add(f'<text x="{x + 28}" y="{ly - 3}" fill="#495057" font-size="12">{escape(label)}</text>')

add('</svg>')
print("\n".join(out))
