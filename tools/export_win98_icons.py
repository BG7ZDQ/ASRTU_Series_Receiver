from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "outputs" / "win98_icon_set_preview_v1.png"
ASSETS = ROOT / "assets" / "icons"


def export_icon(name: str, cell: Image.Image) -> None:
    alpha = cell.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        raise RuntimeError(f"empty icon cell: {name}")
    cropped = cell.crop(bbox)
    side = max(cropped.size)
    # Keep only a very small safety edge. Taskbar icons are rendered at 16–32
    # px, so the former broad transparent margin made every glyph look tiny.
    margin = max(2, side // 64)
    square = Image.new("RGBA", (side + 2 * margin, side + 2 * margin))
    square.alpha_composite(
        cropped,
        ((square.width - cropped.width) // 2,
         (square.height - cropped.height) // 2),
    )
    preview = square.resize((256, 256), Image.Resampling.LANCZOS)
    preview.save(ASSETS / f"{name}.png")
    preview.save(
        ASSETS / f"{name}.ico",
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48),
               (64, 64), (128, 128), (256, 256)],
    )


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    names = ("win98_proxy", "win98_dsp", "win98_doppler")
    if SOURCE.exists():
        image = Image.open(SOURCE).convert("RGBA")
        cell_width = image.width // 3
        cells = [
            image.crop((0, 0, cell_width, image.height)),
            image.crop((cell_width, 0, cell_width * 2, image.height)),
            image.crop((cell_width * 2, 0, image.width, image.height)),
        ]
    else:
        # The repository carries the approved individual PNG masters; the
        # original three-up concept sheet is intentionally not required.
        cells = [Image.open(ASSETS / f"{name}.png").convert("RGBA")
                 for name in names]
    for name, cell in zip(names, cells):
        export_icon(name, cell)

    arrow = Image.new("RGBA", (16, 16))
    draw = ImageDraw.Draw(arrow)
    draw.polygon([(4, 6), (12, 6), (8, 10)], fill=(52, 64, 84, 255))
    arrow.save(ASSETS / "combo_arrow.png")


if __name__ == "__main__":
    main()
