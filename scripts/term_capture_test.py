# Run in scripts/: PYTHONDONTWRITEBYTECODE=1 uv run --with pillow python -m unittest term_capture_test.py
import json
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path

from PIL import Image

from term_capture import encode, load_frames


def encode_six_frames(scenes, last):
    """Encode 6 frames of 100 ms with frame i colored (40 * i, 0, 0) and return the output frames."""
    with tempfile.TemporaryDirectory() as d:
        gif, webp, scenes_json = (Path(d) / n for n in ("in.gif", "out.webp", "scenes.json"))
        imgs = [Image.new("RGB", (64, 32), (i * 40, 0, 0)) for i in range(6)]
        imgs[0].save(gif, save_all=True, append_images=imgs[1:], duration=100, loop=0)
        scenes_json.write_text(json.dumps(scenes))
        encode(Namespace(input=gif, out=webp, scenes=scenes_json, last=last, hold=2000, final_hold=6000,
                         bar_height=34, font_size=17))
        return load_frames(webp)


class EncodeTest(unittest.TestCase):
    def test_cuts_frames_outside_the_scenes_and_pauses_at_scene_ends(self):
        """Scenes start at 1 and 3, --last 4: frames 1-4 remain.
        Frame 2 ends scene 1 and gets 2000 ms. Frame 4 is the last one and gets 6000 ms."""
        out = encode_six_frames([{"start": 1, "caption": "a"}, {"start": 3, "caption": "b", "color": "red"}], 4)
        self.assertEqual([d for _, d in out], [100, 2000, 100, 6000])
        self.assertEqual(out[0][0].size, (64, 66))
        self.assertEqual(out[0][0].getpixel((0, 0)), (40, 0, 0))

    def test_sorts_scenes_and_drops_scenes_after_last(self):
        """Scenes given as 3, 1, 5 with --last 4: scene 5 is dropped, frames 1-4 remain in 2 scenes."""
        out = encode_six_frames([{"start": s, "caption": str(s)} for s in (3, 1, 5)], 4)
        self.assertEqual([d for _, d in out], [100, 2000, 100, 6000])


if __name__ == "__main__":
    unittest.main()
