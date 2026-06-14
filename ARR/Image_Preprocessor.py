"""
process_single_image.py
-----------------------
Single-image variant of ARR_processing_1_.ipynb.

Usage:
    python process_single_image.py <image_path> [--save <output_path>] [--show]

Examples:
    python process_single_image.py photo.jpg
    python process_single_image.py photo.jpg --show
    python process_single_image.py photo.jpg --save cropped.jpg --show
"""

import argparse
import os
import sys

import builtins
import numpy as np
from PIL import Image
from scipy.ndimage import uniform_filter

# ---------------------------------------------------------------------------
# Parameters (copied from notebook)
# ---------------------------------------------------------------------------
color_ranges = [
    (55,  99,  46,  77,  33,  70),   # dark brown
    (81,  122, 68,  115, 58,  99),   # mid brown
    (111, 183, 100, 152, 81,  125),  # light brown
]

WARMTH_RATIO   = 1.3
MAX_AREA       = 0.0211e8
MIN_DIMENSION  = 150
MIN_BRIGHTNESS = 125
DENSITY_KERNEL = 15
DENSITY_THRESH = 0.48


# ---------------------------------------------------------------------------
# Core processing functions (unchanged from notebook)
# ---------------------------------------------------------------------------

def build_combined_mask(img_array: np.ndarray) -> np.ndarray:
    R = img_array[:, :, 0]
    G = img_array[:, :, 1]
    B = img_array[:, :, 2]

    safe_B = np.where(B > 0, B, 1)
    warmth_mask     = (R / safe_B > WARMTH_RATIO) & (B > 0)
    brightness_mask = (R + G + B) >= MIN_BRIGHTNESS

    color_mask = np.zeros(R.shape, dtype=bool)
    for (r0, r1, g0, g1, b0, b1) in color_ranges:
        color_mask |= (
            (R >= r0) & (R <= r1) &
            (G >= g0) & (G <= g1) &
            (B >= b0) & (B <= b1)
        )

    return color_mask & warmth_mask & brightness_mask


def apply_density_filter(mask: np.ndarray, kernel: int, threshold: float) -> np.ndarray:
    """Keep only pixels where >= threshold fraction of the kernel neighbourhood are also brown."""
    density = uniform_filter(mask.astype(float), size=kernel, mode='constant', cval=0)
    return mask & (density >= threshold)


def process_image(image_path: str):
    """
    Load an image, detect the resistor via brown-color masking, and return a
    cropped PIL image centred on the resistor.

    Returns
    -------
    cropped_img : PIL.Image | None
        The cropped image, or None if the resistor could not be detected or
        the crop was too large (mirroring the notebook's rejection logic).
    status : str
        One of 'ok', 'no_color_match', or 'too_large'.
    info : dict
        Diagnostic information.
    """
    with Image.open(image_path) as img:
        img_rgb   = img.convert('RGB')
        img_array = np.array(img_rgb).astype(float)

    combined_mask = build_combined_mask(img_array)
    dense_mask    = apply_density_filter(combined_mask, DENSITY_KERNEL, DENSITY_THRESH)
    matching_coords = np.argwhere(dense_mask)

    if matching_coords.size == 0:
        return None, 'no_color_match', {}

    min_y, min_x = matching_coords.min(axis=0)
    max_y, max_x = matching_coords.max(axis=0)
    img_h, img_w = img_array.shape[:2]

    # Pad to MIN_DIMENSION if needed
    if (max_x - min_x + 1) < MIN_DIMENSION:
        pad   = (MIN_DIMENSION - (max_x - min_x + 1)) // 2
        min_x = builtins.max(0, min_x - pad)
        max_x = builtins.min(img_w - 1, max_x + pad)
        if (max_x - min_x + 1) < MIN_DIMENSION:
            min_x = builtins.max(0, max_x - MIN_DIMENSION + 1)

    if (max_y - min_y + 1) < MIN_DIMENSION:
        pad   = (MIN_DIMENSION - (max_y - min_y + 1)) // 2
        min_y = builtins.max(0, min_y - pad)
        max_y = builtins.min(img_h - 1, max_y + pad)
        if (max_y - min_y + 1) < MIN_DIMENSION:
            min_y = builtins.max(0, max_y - MIN_DIMENSION + 1)

    cropped_img = img_rgb.crop((min_x, min_y, max_x + 1, max_y + 1))
    width, height = cropped_img.size
    area = width * height

    info = {
        'crop_box': (min_x, min_y, max_x + 1, max_y + 1),
        'width':    width,
        'height':   height,
        'area':     area,
    }

    if area > MAX_AREA:
        return cropped_img, 'too_large', info

    return cropped_img, 'ok', info


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Crop a single resistor image using brown-color masking.'
    )
    parser.add_argument('image', help='Path to the input image (JPG / PNG)')
    parser.add_argument('--save', metavar='OUTPUT', default=None,
                        help='Save the cropped image to this path')
    parser.add_argument('--show', action='store_true',
                        help='Display the original and cropped images with matplotlib')
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        print(f"Error: file not found: {args.image}", file=sys.stderr)
        sys.exit(1)

    print(f"Processing: {args.image}")
    cropped, status, info = process_image(args.image)

    if status == 'no_color_match':
        print("Result: NO COLOR MATCH — no brown resistor pixels detected.")
        sys.exit(2)

    if status == 'too_large':
        print(f"Result: TOO LARGE — crop area {info['area']:,} px exceeds limit "
              f"{MAX_AREA:,.0f} px  ({info['width']}×{info['height']}).")
        # Still save / show if requested, so the user can inspect what was found
    else:
        print(f"Result: OK — crop {info['width']}×{info['height']} px  "
              f"(area {info['area']:,} px)")
        print(f"  Crop box (x0, y0, x1, y1): {info['crop_box']}")

    if args.save and cropped is not None:
        cropped.save(args.save)
        print(f"Saved cropped image to: {args.save}")

    if args.show and cropped is not None:
        import matplotlib.pyplot as plt
        import matplotlib.patches as patches

        original = Image.open(args.image).convert('RGB')

        fig, axes = plt.subplots(1, 2, figsize=(12, 5))

        axes[0].imshow(original)
        if info:
            x0, y0, x1, y1 = info['crop_box']
            rect = patches.Rectangle(
                (x0, y0), x1 - x0, y1 - y0,
                linewidth=2, edgecolor='lime', facecolor='none'
            )
            axes[0].add_patch(rect)
        axes[0].set_title(f'Original  ({original.width}×{original.height})')
        axes[0].axis('off')

        axes[1].imshow(cropped)
        axes[1].set_title(
            f'Cropped  ({info["width"]}×{info["height"]})  —  status: {status}'
        )
        axes[1].axis('off')

        plt.suptitle(os.path.basename(args.image), fontsize=11)
        plt.tight_layout()
        plt.show()


if __name__ == '__main__':
    main()
