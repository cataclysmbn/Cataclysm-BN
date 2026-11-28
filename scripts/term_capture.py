#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Turn an asciinema recording into a captioned, lossless animated WebP for a PR.

render CAST OUT.gif                                   render the recording with agg
frames IN.gif DIR                                     save each frame as DIR/<index>.png
encode IN.gif OUT.webp [--scenes S.json] [--last N]   lossless WebP with a caption bar

scenes.json: [{"start": 40, "caption": "unload the bionic scanner"}, ...,
              {"start": 67, "caption": "next turn: invalid ammo error", "color": "red"}]
"start" is a frame index from `frames`. Frames before the first scene and after --last are cut.
The bar shows "N/M  caption". The last frame of each scene stays on screen for --hold ms.
"""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageSequence

AGG = ["agg", "--speed", "2", "--idle-time-limit", "1", "--last-frame-duration", "4",
       "--font-size", "10", "--fps-cap", "4"]
BG, FG, RULE = (40, 42, 54), (248, 248, 242), (90, 92, 110)
COLORS = {"red": (255, 110, 110), "green": (110, 230, 130)}
FONTS = ["/usr/share/fonts/google-noto/NotoSansMono-Bold.ttf",
         "/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf"]


def load_frames(path):
    out = []
    for frame in ImageSequence.Iterator(Image.open(path)):
        frame.load()  # Pillow sets the duration only after load
        out.append((frame.convert("RGB"), int(frame.info.get("duration", 0) or 0)))
    return out


def render(args):
    with tempfile.TemporaryDirectory() as tmp:
        cast = str(Path(tmp) / "v2.cast")  # agg 1.5 reads only asciicast v2
        subprocess.run(["asciinema", "convert", "-f", "asciicast-v2", args.cast, cast], check=True)
        subprocess.run([*AGG, cast, args.out], check=True, stdout=subprocess.DEVNULL)


def frames(args):
    Path(args.dir).mkdir(parents=True, exist_ok=True)
    for i, (img, _) in enumerate(load_frames(args.input)):
        img.save(Path(args.dir) / f"{i:04}.png")


def font(size):
    path = next((p for p in FONTS if Path(p).exists()), None)
    return ImageFont.truetype(path, size) if path else ImageFont.load_default(size)


def encode(args):
    scenes = sorted(json.loads(Path(args.scenes).read_text()) if args.scenes else [], key=lambda s: s["start"])
    if args.last is not None:
        scenes = [s for s in scenes if s["start"] <= args.last]
    first = scenes[0]["start"] if scenes else 0
    src = load_frames(args.input)[first:None if args.last is None else args.last + 1]
    if not src:
        sys.exit(f"no frames between frame {first} and --last {args.last}")
    starts = [s["start"] - first for s in scenes]
    scene_ends = {s - 1 for s in starts[1:]}
    bar = args.bar_height if scenes else 0
    face = font(args.font_size)
    imgs, durations = [], []
    for i, (img, duration) in enumerate(src):
        w, h = img.size
        canvas = Image.new("RGB", (w, h + bar), BG)
        canvas.paste(img, (0, 0))
        if scenes:
            k = max((n for n, s in enumerate(starts) if s <= i), default=0)
            draw = ImageDraw.Draw(canvas)
            draw.line([(0, h), (w, h)], fill=RULE)
            draw.text((12, h + bar // 2), f"{k + 1}/{len(scenes)}  {scenes[k]['caption']}", font=face,
                      fill=COLORS.get(scenes[k].get("color"), FG), anchor="lm")
            duration = args.final_hold if i == len(src) - 1 else args.hold if i in scene_ends else duration
        imgs.append(canvas)
        durations.append(max(duration, 1))
    imgs[0].save(args.out, "WEBP", save_all=True, append_images=imgs[1:], duration=durations, loop=0,
                 lossless=True, quality=100, method=6, minimize_size=True)
    print(f"{args.out}: {len(imgs)} frames, {sum(durations)} ms, {Path(args.out).stat().st_size} B")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(required=True)
    r = sub.add_parser("render")
    r.add_argument("cast")
    r.add_argument("out")
    r.set_defaults(run=render)
    f = sub.add_parser("frames")
    f.add_argument("input")
    f.add_argument("dir")
    f.set_defaults(run=frames)
    e = sub.add_parser("encode")
    e.add_argument("input")
    e.add_argument("out")
    e.add_argument("--scenes")
    e.add_argument("--last", type=int, help="index of the last frame to keep")
    e.add_argument("--hold", type=int, default=2000, help="ms on the last frame of each scene")
    e.add_argument("--final-hold", type=int, default=6000, help="ms on the last frame")
    e.add_argument("--bar-height", type=int, default=34)
    e.add_argument("--font-size", type=int, default=17)
    e.set_defaults(run=encode)
    args = p.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
