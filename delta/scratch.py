import math

width = 80
height = 64

# Create empty grid
grid = [['.' for _ in range(width)] for _ in range(height)]

def draw_line(x0, y0, x1, y1):
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy

    while True:
        if 0 <= x0 < width and 0 <= y0 < height:
            grid[y0][x0] = '#'
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0 += sx
        if e2 < dx:
            err += dx
            y0 += sy

def fill_triangle(x1, y1, x2, y2, x3, y3):
    # simple bounding box fill
    min_x = max(0, min(x1, x2, x3))
    max_x = min(width - 1, max(x1, x2, x3))
    min_y = max(0, min(y1, y2, y3))
    max_y = min(height - 1, max(y1, y2, y3))
    
    def sign(p1x, p1y, p2x, p2y, p3x, p3y):
        return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y)

    for y in range(min_y, max_y + 1):
        for x in range(min_x, max_x + 1):
            d1 = sign(x, y, x1, y1, x2, y2)
            d2 = sign(x, y, x2, y2, x3, y3)
            d3 = sign(x, y, x3, y3, x1, y1)

            has_neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
            has_pos = (d1 > 0) or (d2 > 0) or (d3 > 0)

            if not (has_neg and has_pos):
                grid[y][x] = '#'

# Large outer delta
draw_line(40, 5, 10, 45)
draw_line(40, 5, 70, 45)
draw_line(10, 45, 70, 45)

# Thicken it
draw_line(40, 6, 11, 45)
draw_line(40, 6, 69, 45)
draw_line(11, 44, 69, 44)

draw_line(40, 7, 12, 45)
draw_line(40, 7, 68, 45)
draw_line(12, 43, 68, 43)

# Inner solid delta
fill_triangle(40, 18, 25, 38, 55, 38)

# Add "DELTA" text at the bottom
delta_text = [
"  ###   ##### #     #####   #   ",
"  #  #  #     #       #    # #  ",
"  #  #  ###   #       #   ##### ",
"  #  #  #     #       #   #   # ",
"  ###   ##### #####   #   #   # "
]

start_y = 52
start_x = (80 - len(delta_text[0])) // 2

for i, row in enumerate(delta_text):
    y = start_y + i
    if y < height:
        for j, char in enumerate(row):
            x = start_x + j
            if x < width:
                grid[y][x] = '#' if char == '#' else '.'

hex_bytes = []
for y in range(64):
    for x_byte in range(10):
        val = 0
        for bit in range(8):
            x = x_byte * 8 + bit
            if grid[y][x] == '#':
                val |= (1 << bit)
        hex_bytes.append(val)

print("const unsigned char logo[] PROGMEM = {")
for i in range(0, len(hex_bytes), 16):
    chunk = hex_bytes[i:i+16]
    print("  " + ",".join([f"0x{b:02X}" for b in chunk]) + ",")
print("};")

print("\n--- DEMO ---")
for row in grid:
    print(''.join(row))
