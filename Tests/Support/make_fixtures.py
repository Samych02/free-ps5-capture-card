#!/usr/bin/env python3
"""Encode test-only A/V. HDR is generated from absolute luminance, not retagged SDR."""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

W, H, FPS, SECONDS = 320, 180, 30, 10
RECIPE = 2


def pq_code(nits):
    m1, m2, c1, c2, c3 = 2610/16384, 2523/32, 3424/4096, 2413/128, 2392/128
    y = (nits / 10000) ** m1
    return round(64 + 876 * ((c1 + c2*y) / (1 + c3*y)) ** m2)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "manifest.json"
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text())
        if previous.get("recipe") == RECIPE and all(
            (out / n).is_file() and digest(out / n) == h
            for n, h in previous.get("files", {}).items()
        ) and len(previous.get("files", {})) == 3:
            return
    manifest = {"recipe": RECIPE, "files": {}, "hdr_reference_nits": [0, 10, 100, 1000],
                "hdr_reference_limited_y10": [pq_code(x) for x in (0, 10, 100, 1000)]}
    audio = r"aevalsrc=if(lt(mod(t\,1)\,0.05)\,0.2*sin(2*PI*1000*t)\,0):s=48000"
    for mode in ("h264-opus", "hevc-opus", "hevc-hdr-opus"):
        hdr = "hdr" in mode
        path = out / (mode + ".mkv")
        cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
        if hdr:
            cmd += ["-f", "rawvideo", "-pixel_format", "yuv420p10le", "-video_size", f"{W}x{H}",
                    "-framerate", str(FPS), "-i", "pipe:0"]
        else:
            video = f"testsrc2=size={W}x{H}:rate={FPS},drawbox=color=white:t=fill:enable='lt(mod(t,1),0.05)'"
            cmd += ["-f", "lavfi", "-i", video]
        cmd += ["-f", "lavfi", "-i", audio, "-map", "0:v:0", "-map", "1:a:0",
                "-t", str(SECONDS), "-c:v", "libx264" if mode.startswith("h264") else "libx265",
                "-preset", "ultrafast", "-bf", "0", "-g", str(FPS),
                "-pix_fmt", "yuv420p10le" if hdr else "yuv420p", "-color_range", "tv",
                "-color_primaries", "bt2020" if hdr else "bt709",
                "-color_trc", "smpte2084" if hdr else "bt709",
                "-colorspace", "bt2020nc" if hdr else "bt709"]
        if not mode.startswith("h264"):
            params = "pools=1:frame-threads=1:log-level=error:scenecut=0:colorprim=" + ("9:transfer=16:colormatrix=9" if hdr else "1:transfer=1:colormatrix=1")
            if hdr:
                params += ":hdr10=1:master-display=G(8500,39850)B(6550,2300)R(35400,14600)WP(15635,16450)L(10000000,1):max-cll=1000,1000"
            cmd += ["-x265-params", params]
        cmd += ["-c:a", "libopus", "-b:a", "96k", "-ac", "2", str(path)]
        if hdr:
            # Neutral BT.2020 grayscale patches with exact PQ-encoded nits.
            # The first two frames each second flash at 1000 nits in sync
            # with the 50 ms audio impulse. Stored samples are little-endian.
            def frame(flash):
                levels = manifest["hdr_reference_limited_y10"]
                row = [levels[3] if flash else levels[min(3, x * 4 // W)] for x in range(W)]
                pixels = array("H", row * H + [512] * (W * H // 2))
                if sys.byteorder != "little": pixels.byteswap()
                return pixels.tobytes()
            base, flash = frame(False), frame(True)
            with (out / "hdr-encode.log").open("wb") as log:
                process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=log)
                try:
                    for index in range(FPS * SECONDS):
                        process.stdin.write(flash if index % FPS < 2 else base)
                finally:
                    process.stdin.close()
                if process.wait(timeout=90):
                    raise RuntimeError(f"HDR fixture encoding failed; see {out / 'hdr-encode.log'}")
        else:
            subprocess.run(cmd, check=True, timeout=90)
        manifest["files"][path.name] = digest(path)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    generate(parser.parse_args().out)
