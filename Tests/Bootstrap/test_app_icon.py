"""Validate the complete native macOS icon set without image dependencies."""
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AppIconTests(unittest.TestCase):
    def test_native_icon_has_every_macos_size(self):
        folder = ROOT / "App/Assets.xcassets/AppIcon.appiconset"
        catalog = json.loads((folder / "Contents.json").read_text())
        expected = {(size, scale) for size in (16, 32, 128, 256, 512) for scale in (1, 2)}
        actual = set()
        for image in catalog["images"]:
            self.assertEqual(image["idiom"], "mac")
            size = int(image["size"].split("x")[0])
            scale = int(image["scale"].removesuffix("x"))
            actual.add((size, scale))
            data = (folder / image["filename"]).read_bytes()
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            self.assertEqual(struct.unpack(">II", data[16:24]), (size * scale, size * scale))
        self.assertEqual(actual, expected)
        self.assertEqual(len(catalog["images"]), len(expected))

    def test_app_build_selects_icon(self):
        self.assertIn("ASSETCATALOG_COMPILER_APPICON_NAME: AppIcon", (ROOT / "project.yml").read_text())


if __name__ == "__main__":
    unittest.main()
