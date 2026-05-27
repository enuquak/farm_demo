"""
Generate tileset and player sprite assets for the farm game.
Run this script once to create the PNG files.
"""
import pygame
import os

TILE_SIZE = 16


def generate_tileset():
    """Generate a 16x16 tileset PNG with ground and object tiles."""
    pygame.init()

    # Tile layout: 8 tiles per row
    # Row 0: Ground tiles (grass, dirt, water, sand, tilled, stone_floor, wood_floor, path)
    # Row 1: Object tiles (stone, crop_growing, crop_ready, tree_trunk, tree_top, door, bed, tv)
    # Row 2: More objects (stove, fence_h, fence_v, flower, bush, sign, well, bridge)

    cols = 8
    rows = 3
    surface = pygame.Surface((cols * TILE_SIZE, rows * TILE_SIZE), pygame.SRCALPHA)
    surface.fill((0, 0, 0, 0))

    # Helper to draw a tile at grid position
    def draw_tile(col, row, pixels):
        """Draw a 16x16 tile from a list of (x, y, color) tuples."""
        base_x = col * TILE_SIZE
        base_y = row * TILE_SIZE
        for x, y, color in pixels:
            if 0 <= x < TILE_SIZE and 0 <= y < TILE_SIZE:
                surface.set_at((base_x + x, base_y + y), (*color, 255))

    def fill_tile(col, row, color):
        """Fill entire tile with a solid color."""
        base_x = col * TILE_SIZE
        base_y = row * TILE_SIZE
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                surface.set_at((base_x + x, base_y + y), (*color, 255))

    def fill_tile_with_noise(col, row, base_color, noise_color, density=0.15):
        """Fill tile with base color and random noise dots."""
        import random
        random.seed(col * 100 + row)
        base_x = col * TILE_SIZE
        base_y = row * TILE_SIZE
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                if random.random() < density:
                    surface.set_at((base_x + x, base_y + y), (*noise_color, 255))
                else:
                    surface.set_at((base_x + x, base_y + y), (*base_color, 255))

    # === Row 0: Ground tiles ===

    # 0,0: Grass
    fill_tile_with_noise(0, 0, (74, 124, 46), (60, 110, 35), 0.2)

    # 1,0: Dirt
    fill_tile_with_noise(1, 0, (139, 90, 43), (120, 75, 35), 0.15)

    # 2,0: Water
    fill_tile_with_noise(2, 0, (30, 90, 160), (50, 120, 190), 0.25)

    # 3,0: Sand
    fill_tile_with_noise(3, 0, (210, 180, 120), (195, 165, 105), 0.15)

    # 4,0: Tilled soil
    fill_tile(4, 0, (100, 60, 30))
    # Add furrow lines
    base_x = 4 * TILE_SIZE
    base_y = 0 * TILE_SIZE
    for y in range(0, TILE_SIZE, 4):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (80, 48, 24, 255))

    # 5,0: Stone floor
    fill_tile_with_noise(5, 0, (140, 140, 140), (120, 120, 120), 0.1)
    # Add grid lines
    base_x = 5 * TILE_SIZE
    base_y = 0 * TILE_SIZE
    for x in range(TILE_SIZE):
        surface.set_at((base_x + x, base_y + 0), (100, 100, 100, 255))
        surface.set_at((base_x + x, base_y + 7), (100, 100, 100, 255))
    for y in range(TILE_SIZE):
        surface.set_at((base_x + 0, base_y + y), (100, 100, 100, 255))
        surface.set_at((base_x + 7, base_y + y), (100, 100, 100, 255))

    # 6,0: Wood floor
    fill_tile(6, 0, (160, 110, 60))
    base_x = 6 * TILE_SIZE
    base_y = 0 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(0, TILE_SIZE, 5):
            surface.set_at((base_x + x, base_y + y), (140, 95, 50, 255))

    # 7,0: Path
    fill_tile_with_noise(7, 0, (180, 160, 120), (165, 145, 105), 0.1)

    # === Row 1: Object tiles ===

    # 0,1: Stone (gray rock)
    fill_tile(0, 1, (0, 0, 0))  # transparent background
    stone_color = (128, 128, 128)
    stone_dark = (96, 96, 96)
    stone_light = (160, 160, 160)
    pixels = []
    # Simple stone shape
    for y in range(4, 13):
        for x in range(3, 13):
            if y == 4 and (x < 5 or x > 10):
                continue
            if y == 12 and (x < 4 or x > 11):
                continue
            c = stone_light if y < 8 else stone_dark
            if x == 3 or x == 12 or y == 4 or y == 12:
                c = stone_color
            pixels.append((x, y, c))
    # Clear and redraw
    base_x = 0 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    for x, y, c in pixels:
        surface.set_at((base_x + x, base_y + y), (*c, 255))

    # 1,1: Crop growing (small green sprout)
    base_x = 1 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Stem
    for y in range(8, 14):
        surface.set_at((base_x + 7, base_y + y), (101, 67, 33, 255))
        surface.set_at((base_x + 8, base_y + y), (101, 67, 33, 255))
    # Leaves
    for y in range(5, 8):
        for x in range(6, 10):
            surface.set_at((base_x + x, base_y + y), (50, 180, 50, 255))

    # 2,1: Crop ready (golden grain)
    base_x = 2 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Grain head
    for y in range(3, 10):
        for x in range(5, 11):
            surface.set_at((base_x + x, base_y + y), (255, 215, 0, 255))
    # Stem
    for y in range(10, 15):
        surface.set_at((base_x + 7, base_y + y), (101, 67, 33, 255))
        surface.set_at((base_x + 8, base_y + y), (101, 67, 33, 255))

    # 3,1: Tree trunk (bottom half of tree, walkable area marker)
    base_x = 3 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Trunk
    for y in range(0, TILE_SIZE):
        for x in range(6, 10):
            surface.set_at((base_x + x, base_y + y), (101, 67, 33, 255))

    # 4,1: Tree top (green canopy, renders over player)
    base_x = 4 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Canopy circle
    cx, cy, r = 8, 7, 6
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            dist = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if dist <= r:
                shade = 50 + int(20 * (r - dist) / r)
                surface.set_at((base_x + x, base_y + y), (0, 100 + shade, 0, 255))

    # 5,1: Door
    base_x = 5 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Door frame
    for y in range(2, 15):
        for x in range(3, 13):
            if x == 3 or x == 12 or y == 2:
                surface.set_at((base_x + x, base_y + y), (101, 67, 33, 255))
            elif y >= 3:
                surface.set_at((base_x + x, base_y + y), (160, 82, 45, 255))
    # Doorknob
    surface.set_at((base_x + 10, base_y + 8), (200, 180, 50, 255))

    # 6,1: Bed
    base_x = 6 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Bed frame
    for y in range(2, 14):
        for x in range(1, 15):
            if y == 2 or y == 13 or x == 1 or x == 14:
                surface.set_at((base_x + x, base_y + y), (139, 69, 19, 255))
            else:
                surface.set_at((base_x + x, base_y + y), (255, 255, 255, 255))
    # Pillow
    for y in range(3, 6):
        for x in range(2, 6):
            surface.set_at((base_x + x, base_y + y), (200, 200, 255, 255))

    # 7,1: TV
    base_x = 7 * TILE_SIZE
    base_y = 1 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # TV body
    for y in range(2, 10):
        for x in range(2, 14):
            surface.set_at((base_x + x, base_y + y), (30, 30, 30, 255))
    # Screen
    for y in range(3, 9):
        for x in range(3, 13):
            surface.set_at((base_x + x, base_y + y), (100, 149, 237, 255))
    # Stand
    for y in range(10, 13):
        for x in range(6, 10):
            surface.set_at((base_x + x, base_y + y), (80, 80, 80, 255))

    # === Row 2: Additional objects ===

    # 0,2: Stove
    base_x = 0 * TILE_SIZE
    base_y = 2 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    for y in range(3, 13):
        for x in range(2, 14):
            if y == 3 or y == 12 or x == 2 or x == 13:
                surface.set_at((base_x + x, base_y + y), (80, 80, 80, 255))
            else:
                surface.set_at((base_x + x, base_y + y), (169, 169, 169, 255))
    # Burner
    for y in range(5, 8):
        for x in range(4, 7):
            surface.set_at((base_x + x, base_y + y), (50, 50, 50, 255))
    for y in range(5, 8):
        for x in range(9, 12):
            surface.set_at((base_x + x, base_y + y), (50, 50, 50, 255))

    # 1,2: Fence horizontal
    base_x = 1 * TILE_SIZE
    base_y = 2 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Horizontal bar
    for x in range(TILE_SIZE):
        surface.set_at((base_x + x, base_y + 5), (139, 90, 43, 255))
        surface.set_at((base_x + x, base_y + 6), (139, 90, 43, 255))
        surface.set_at((base_x + x, base_y + 10), (139, 90, 43, 255))
        surface.set_at((base_x + x, base_y + 11), (139, 90, 43, 255))
    # Posts
    for y in range(2, 15):
        surface.set_at((base_x + 2, base_y + y), (120, 78, 38, 255))
        surface.set_at((base_x + 3, base_y + y), (120, 78, 38, 255))
        surface.set_at((base_x + 12, base_y + y), (120, 78, 38, 255))
        surface.set_at((base_x + 13, base_y + y), (120, 78, 38, 255))

    # 2,2: Flower
    base_x = 2 * TILE_SIZE
    base_y = 2 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    # Stem
    for y in range(8, 15):
        surface.set_at((base_x + 7, base_y + y), (0, 128, 0, 255))
        surface.set_at((base_x + 8, base_y + y), (0, 128, 0, 255))
    # Petals
    petal_color = (255, 100, 150)
    cx, cy = 8, 6
    for dx, dy in [(0, -2), (0, 2), (-2, 0), (2, 0)]:
        surface.set_at((base_x + cx + dx, base_y + cy + dy), (*petal_color, 255))
        surface.set_at((base_x + cx + dx + 1, base_y + cy + dy), (*petal_color, 255))
        surface.set_at((base_x + cx + dx, base_y + cy + dy + 1), (*petal_color, 255))
    # Center
    surface.set_at((base_x + 7, base_y + 5), (255, 255, 0, 255))
    surface.set_at((base_x + 8, base_y + 5), (255, 255, 0, 255))
    surface.set_at((base_x + 7, base_y + 6), (255, 255, 0, 255))
    surface.set_at((base_x + 8, base_y + 6), (255, 255, 0, 255))

    # 3,2: Bush
    base_x = 3 * TILE_SIZE
    base_y = 2 * TILE_SIZE
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            surface.set_at((base_x + x, base_y + y), (0, 0, 0, 0))
    cx, cy, r = 8, 9, 5
    for y in range(TILE_SIZE):
        for x in range(TILE_SIZE):
            dist = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if dist <= r:
                shade = int(30 * (r - dist) / r)
                surface.set_at((base_x + x, base_y + y), (30 + shade, 100 + shade, 20 + shade, 255))

    # Save tileset
    output_dir = os.path.join(os.path.dirname(__file__), "assets", "tilesets")
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, "farm_tileset.png")
    pygame.image.save(surface, output_path)
    print(f"Tileset saved to {output_path} ({surface.get_width()}x{surface.get_height()})")
    return output_path


def generate_player_sprite():
    """Generate a player sprite sheet: 4 directions x 4 frames, 16x16 each."""
    pygame.init()

    frame_w = TILE_SIZE
    frame_h = TILE_SIZE
    cols = 4  # 4 frames per direction
    rows = 4  # 4 directions (down, left, right, up)

    surface = pygame.Surface((cols * frame_w, rows * frame_h), pygame.SRCALPHA)
    surface.fill((0, 0, 0, 0))

    # Color palette
    skin = (255, 200, 160)
    hair = (100, 60, 30)
    shirt = (50, 120, 200)
    pants = (80, 80, 140)
    shoes = (60, 40, 30)
    outline = (40, 30, 20)

    def draw_character(col, row, direction, frame):
        """Draw a character frame."""
        base_x = col * frame_w
        base_y = row * frame_h

        # Walk offset for animation (slight horizontal shift)
        walk_offset = 0
        leg_offset = 0
        if frame == 1:
            walk_offset = -1
            leg_offset = 1
        elif frame == 2:
            walk_offset = 0
            leg_offset = 0
        elif frame == 3:
            walk_offset = 1
            leg_offset = -1

        # Head position
        head_y = 1

        # Draw based on direction
        if direction == "down":
            # Hair (top of head)
            for y in range(head_y, head_y + 3):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*hair, 255))
            # Face
            for y in range(head_y + 3, head_y + 6):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*skin, 255))
            # Eyes
            surface.set_at((base_x + 6 + walk_offset, base_y + head_y + 4), (*outline, 255))
            surface.set_at((base_x + 9 + walk_offset, base_y + head_y + 4), (*outline, 255))
            # Body (shirt)
            for y in range(head_y + 6, head_y + 10):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*shirt, 255))
            # Pants
            for y in range(head_y + 10, head_y + 13):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*pants, 255))
            # Legs (walking animation)
            left_leg_x = 6 + walk_offset
            right_leg_x = 9 + walk_offset
            leg_y = head_y + 13
            surface.set_at((base_x + left_leg_x, base_y + leg_y), (*pants, 255))
            surface.set_at((base_x + left_leg_x, base_y + leg_y + 1), (*shoes, 255))
            surface.set_at((base_x + right_leg_x + leg_offset, base_y + leg_y), (*pants, 255))
            surface.set_at((base_x + right_leg_x + leg_offset, base_y + leg_y + 1), (*shoes, 255))

        elif direction == "up":
            # Hair (back of head, full coverage)
            for y in range(head_y, head_y + 6):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*hair, 255))
            # Body
            for y in range(head_y + 6, head_y + 10):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*shirt, 255))
            # Pants
            for y in range(head_y + 10, head_y + 13):
                for x in range(5 + walk_offset, 11 + walk_offset):
                    surface.set_at((base_x + x, base_y + y), (*pants, 255))
            # Legs
            left_leg_x = 6 + walk_offset
            right_leg_x = 9 + walk_offset
            leg_y = head_y + 13
            surface.set_at((base_x + left_leg_x, base_y + leg_y), (*pants, 255))
            surface.set_at((base_x + left_leg_x, base_y + leg_y + 1), (*shoes, 255))
            surface.set_at((base_x + right_leg_x + leg_offset, base_y + leg_y), (*pants, 255))
            surface.set_at((base_x + right_leg_x + leg_offset, base_y + leg_y + 1), (*shoes, 255))

        elif direction == "left":
            # Side view - facing left
            # Hair
            for y in range(head_y, head_y + 3):
                for x in range(6, 11):
                    surface.set_at((base_x + x, base_y + y), (*hair, 255))
            # Face (side)
            for y in range(head_y + 3, head_y + 6):
                for x in range(6, 10):
                    surface.set_at((base_x + x, base_y + y), (*skin, 255))
            # Eye
            surface.set_at((base_x + 7, base_y + head_y + 4), (*outline, 255))
            # Body
            for y in range(head_y + 6, head_y + 10):
                for x in range(6, 11):
                    surface.set_at((base_x + x, base_y + y), (*shirt, 255))
            # Pants
            for y in range(head_y + 10, head_y + 13):
                for x in range(6, 11):
                    surface.set_at((base_x + x, base_y + y), (*pants, 255))
            # Legs with walk animation
            leg_y = head_y + 13
            surface.set_at((base_x + 7, base_y + leg_y + leg_offset), (*pants, 255))
            surface.set_at((base_x + 7, base_y + leg_y + leg_offset + 1), (*shoes, 255))
            surface.set_at((base_x + 9, base_y + leg_y - leg_offset), (*pants, 255))
            surface.set_at((base_x + 9, base_y + leg_y - leg_offset + 1), (*shoes, 255))

        elif direction == "right":
            # Side view - facing right (mirrored)
            # Hair
            for y in range(head_y, head_y + 3):
                for x in range(5, 10):
                    surface.set_at((base_x + x, base_y + y), (*hair, 255))
            # Face (side)
            for y in range(head_y + 3, head_y + 6):
                for x in range(6, 10):
                    surface.set_at((base_x + x, base_y + y), (*skin, 255))
            # Eye
            surface.set_at((base_x + 9, base_y + head_y + 4), (*outline, 255))
            # Body
            for y in range(head_y + 6, head_y + 10):
                for x in range(5, 10):
                    surface.set_at((base_x + x, base_y + y), (*shirt, 255))
            # Pants
            for y in range(head_y + 10, head_y + 13):
                for x in range(5, 10):
                    surface.set_at((base_x + x, base_y + y), (*pants, 255))
            # Legs with walk animation
            leg_y = head_y + 13
            surface.set_at((base_x + 7, base_y + leg_y - leg_offset), (*pants, 255))
            surface.set_at((base_x + 7, base_y + leg_y - leg_offset + 1), (*shoes, 255))
            surface.set_at((base_x + 9, base_y + leg_y + leg_offset), (*pants, 255))
            surface.set_at((base_x + 9, base_y + leg_y + leg_offset + 1), (*shoes, 255))

    # Draw all frames
    directions = ["down", "left", "right", "up"]
    for row, direction in enumerate(directions):
        for col in range(4):
            draw_character(col, row, direction, col)

    # Save
    output_dir = os.path.join(os.path.dirname(__file__), "assets", "sprites")
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, "player.png")
    pygame.image.save(surface, output_path)
    print(f"Player sprite saved to {output_path} ({surface.get_width()}x{surface.get_height()})")
    return output_path


if __name__ == "__main__":
    generate_tileset()
    generate_player_sprite()
    print("All assets generated successfully.")
