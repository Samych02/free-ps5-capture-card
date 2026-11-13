#!/usr/bin/env python3
"""Black-box transport tests; synthetic encoders/validation decoders are test-only."""
import argparse
import json
from pathlib import Path
import socket
import selectors
import subprocess
import sys
import tempfile
import time
import unittest
from array import array

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build/native"
MODES = {"h264-opus": "h264", "hevc-opus": "hevc", "hevc-hdr-opus": "hevc"}


def run(*args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, **kwargs)


def probe(path, packets=False):
    args = ["ffprobe", "-v", "error", "-show_streams", "-of", "json"]
    if packets:
        args += ["-show_packets", "-show_data_hash", "sha256"]
    return json.loads(run(*args, str(path)).stdout)


def first_frame(path):
    return json.loads(run("ffprobe", "-v", "error", "-select_streams", "v:0",
                          "-read_intervals", "%+0.1", "-show_frames", "-of", "json",
                          str(path)).stdout)["frames"][0]


class ClockTests(unittest.TestCase):
    def test_two_hour_clock_and_owned_queue(self):
        for binary, count in (("capture-timeline-tests", 5), ("capture-queue-tests", 12)):
            with self.subTest(suite=binary):
                result = run(str(BUILD / "Tests/Native" / binary), timeout=30)
                self.assertIn(f"{count} of {count} (100%) tests successful".encode(), result.stdout)


class RemuxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generator = ROOT / "Tests/Support/make_fixtures.py"
        cls.replay = BUILD / "bin/capture-replay"
        if cls.generator.exists():
            run(sys.executable, str(cls.generator), "--out", str(BUILD / "fixtures"))

    def test_required_fixtures_have_video_audio_and_real_hdr(self):
        self.assertTrue(self.generator.exists(), "synthetic fixture generator is not implemented")
        for name, codec in MODES.items():
            with self.subTest(mode=name):
                streams = probe(BUILD / "fixtures" / (name + ".mkv"))["streams"]
                self.assertEqual([s["codec_type"] for s in streams], ["video", "audio"])
                self.assertEqual(streams[0]["codec_name"], codec)
                self.assertEqual(streams[1]["codec_name"], "opus")
                self.assertEqual(streams[1]["channels"], 2)
                if "hdr" in name:
                    self.assertEqual(streams[0]["pix_fmt"], "yuv420p10le")
                    # Stream-only probing may omit fields resolved by parsing
                    # the first frame. Verify actual decoded signaling, too.
                    frame = first_frame(BUILD / "fixtures" / (name + ".mkv"))
                    self.assertEqual(frame["color_transfer"], "smpte2084")
                    self.assertEqual(frame["color_primaries"], "bt2020")
                    self.assertEqual(frame["color_space"], "bt2020nc")
                    side = {d["side_data_type"]: d for d in frame["side_data_list"]}
                    self.assertEqual(side["Content light level metadata"]["max_content"], 1000)
                    self.assertIn("Mastering display metadata", side)
                    raw = run("ffmpeg", "-v", "error", "-ss", "0.2", "-i",
                              str(BUILD / "fixtures" / (name + ".mkv")), "-frames:v", "1",
                              "-pix_fmt", "yuv420p10le", "-f", "rawvideo", "pipe:1").stdout
                    pixels = array("H")
                    pixels.frombytes(raw)
                    if sys.byteorder != "little": pixels.byteswap()
                    expected = json.loads((BUILD / "fixtures/manifest.json").read_text())["hdr_reference_limited_y10"]
                    for x, value in zip((40, 120, 200, 280), expected):
                        self.assertLessEqual(abs(pixels[50 * 320 + x] - value), 8)

    def test_nonseekable_remux_preserves_both_compressed_tracks(self):
        self.assertTrue(self.replay.exists(), "native compressed remux/replay is not implemented")
        for name in MODES:
            with self.subTest(mode=name), tempfile.TemporaryDirectory() as tmp:
                source = BUILD / "fixtures" / (name + ".mkv")
                self.assertTrue(source.exists(), "generate fixtures first")
                out = Path(tmp) / "remux.mkv"
                run(str(self.replay), "--fixture", str(source), "--output", str(out))
                before, after = probe(source, True), probe(out, True)
                self.assertEqual(len(after["streams"]), 2)
                # Fixture containers already carry length-prefixed NALs. Exact
                # payload hashes are stronger than normalization for this case;
                # live Annex-B conversion is a separate later receiver test.
                for track in (0, 1):
                    original = [p["data_hash"] for p in before["packets"] if p["stream_index"] == track]
                    remuxed = [p["data_hash"] for p in after["packets"] if p["stream_index"] == track]
                    self.assertEqual(original, remuxed)
                    pts = [float(p["pts_time"]) for p in after["packets"] if p["stream_index"] == track]
                    self.assertTrue(all(b >= a for a, b in zip(pts, pts[1:])))
                run("ffmpeg", "-v", "error", "-xerror", "-i", str(out), "-f", "null", "-")
                if "hdr" in name:
                    frame = first_frame(out)
                    self.assertEqual(frame["color_transfer"], "smpte2084")
                    self.assertEqual(frame["color_primaries"], "bt2020")

    def test_live_annexb_metadata_without_decoder(self):
        for mode, name in enumerate(MODES):
            with self.subTest(mode=name):
                metadata = json.loads(run(str(BUILD / "bin/capture-live-format"), str(BUILD / "fixtures" / (name + ".mkv")), str(mode)).stdout)
                self.assertEqual((metadata["width"],metadata["height"]),(320,180))
                self.assertEqual(metadata["fps_num"] / metadata["fps_den"],30)
                self.assertEqual(metadata["depth"],10 if mode == 2 else 8)
                self.assertEqual(metadata["hdr"],mode == 2)
                if mode == 2:
                    self.assertEqual(metadata["primaries"],9)
                    self.assertEqual(metadata["transfer"],16)
                    self.assertGreaterEqual(metadata["side_data"],2)

    def test_live_annexb_opus_remux(self):
        for mode, name in enumerate(MODES):
            with self.subTest(mode=name), tempfile.TemporaryDirectory() as tmp:
                source = BUILD / "fixtures" / (name + ".mkv")
                output = Path(tmp) / "live.mkv"
                run(str(BUILD / "bin/capture-live-format"),str(source),str(mode),str(output))
                streams = probe(output)["streams"]
                self.assertEqual([s["codec_type"] for s in streams],["video","audio"])
                before,after=probe(source,True),probe(output,True)
                self.assertEqual([p["data_hash"] for p in before["packets"] if p["stream_index"]==1],
                                 [p["data_hash"] for p in after["packets"] if p["stream_index"]==1])
                def vcl(path):
                    raw=run("ffmpeg","-v","error","-i",str(path),"-map","0:v:0","-c","copy","-bsf:v",
                            "h264_mp4toannexb" if mode==0 else "hevc_mp4toannexb","-f","h264" if mode==0 else "hevc","pipe:1").stdout
                    import re
                    units=re.split(b"\x00\x00(?:\x00)?\x01",raw)
                    return [n.rstrip(b"\0") for n in units if n and ((n[0]&31) in (1,2,3,4,5) if mode==0 else ((n[0]>>1)&63)<32)]
                self.assertEqual(vcl(source),vcl(output))
                run("ffmpeg","-v","error","-xerror","-i",str(output),"-f","null","-")
                frame=first_frame(output)
                self.assertEqual(frame["color_primaries"],"bt2020" if mode==2 else "bt709")
                if mode==2:
                    self.assertEqual(frame["color_transfer"],"smpte2084")
                    side={d["side_data_type"]:d for d in frame["side_data_list"]}
                    self.assertEqual(side["Content light level metadata"]["max_content"],1000)

    def test_live_reordering_and_hdr_mismatch_are_rejected(self):
        tool=str(BUILD / "bin/capture-live-format")
        result=subprocess.run([tool,str(BUILD / "fixtures/hevc-opus.mkv"),"2"],capture_output=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(b"HDR modes do not match",result.stderr)
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp) / "reordered.mkv"
            run("ffmpeg","-v","error","-f","lavfi","-i","testsrc2=size=320x180:rate=30","-t","0.5",
                "-c:v","libx264","-bf","2","-an",str(source))
            result=subprocess.run([tool,str(source),"0"],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn(b"reorder",result.stderr.lower())
        # Public mux entry must reject before cloning/copying an oversized
        # packet whose claimed data pointer is deliberately not readable.
        run(tool,str(BUILD / "fixtures/h264-opus.mkv"),"0","--check-bounds")

    def test_tcp_streams_both_tracks_with_fresh_headers_on_reconnect(self):
        self.assertTrue(self.replay.exists(), "native replay is not implemented")
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        for name in MODES:
            with self.subTest(mode=name), tempfile.TemporaryDirectory() as tmp:
                process = subprocess.Popen([str(self.replay), "--fixture",
                    str(BUILD / "fixtures" / (name + ".mkv")), "--loop", "--port", str(port)],
                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
                try:
                    with selectors.DefaultSelector() as selector:
                        selector.register(process.stderr, selectors.EVENT_READ)
                        self.assertTrue(selector.select(5), "server did not announce readiness")
                        ready = process.stderr.readline().decode()
                    self.assertIn(f"READY 127.0.0.1:{port}", ready)
                    with socket.create_connection(("127.0.0.1", port), timeout=3) as primary:
                        self.assertTrue(primary.recv(4))
                        with socket.create_connection(("127.0.0.1", port), timeout=1) as second:
                            self.assertEqual(second.recv(4), b"", "second active consumer must be rejected")
                    output = Path(tmp) / "tcp.mkv"
                    duration = "11" if name == "hevc-opus" else "1"
                    run("ffmpeg", "-v", "error", "-y", "-analyzeduration", "100000",
                        "-probesize", "65536", "-f", "matroska", "-i", f"tcp://127.0.0.1:{port}",
                        "-t", duration, "-map", "0", "-c", "copy", str(output), timeout=20)
                    streams = probe(output)["streams"]
                    self.assertEqual([s["codec_type"] for s in streams], ["video", "audio"])
                    if name == "hevc-opus":
                        packets = probe(output, True)["packets"]
                        for track in (0, 1):
                            pts = [float(p["pts_time"]) for p in packets if p["stream_index"] == track]
                            self.assertGreater(pts[-1], 10)
                            self.assertTrue(all(b >= a for a, b in zip(pts, pts[1:])), "loop rewound timestamps")
                    if "hdr" in name:
                        self.assertEqual(first_frame(output)["color_transfer"], "smpte2084")
                    for _ in range(10):
                        # Peer close and accept occur on separate TCP connections.
                        # A reconnect arriving before old-peer cleanup is rejected
                        # like any second consumer. Verify bounded eventual recovery,
                        # not an assumed instantaneous close notification.
                        deadline, header = time.monotonic() + 3, b""
                        while time.monotonic() < deadline and len(header) < 4:
                            header = b""
                            with socket.create_connection(("127.0.0.1", port), timeout=1) as client:
                                while len(header) < 4:
                                    try:
                                        block = client.recv(4 - len(header))
                                    except ConnectionResetError:
                                        break
                                    if not block: break
                                    header += block
                        self.assertEqual(header, bytes.fromhex("1a45dfa3"))
                    self.assertIsNone(process.poll(), "server died during reconnects")
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                        self.fail("server failed to stop within 2 seconds")
                    process.stderr.close()

    def test_receiver_pipeline_delivers_both_tracks(self):
        for mode,name in enumerate(MODES):
            with self.subTest(mode=name), tempfile.TemporaryDirectory() as tmp:
                with socket.socket() as reserve:
                    reserve.bind(("127.0.0.1",0)); port=reserve.getsockname()[1]
                process=subprocess.Popen([str(BUILD / "bin/capture-pipeline-fixture"),str(BUILD / "fixtures" / (name+".mkv")),str(mode),str(port)],
                                         stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
                try:
                    with selectors.DefaultSelector() as selector:
                        selector.register(process.stderr,selectors.EVENT_READ)
                        self.assertTrue(selector.select(5)); self.assertIn(b"READY",process.stderr.readline())
                    output=Path(tmp) / "pipeline.mkv"
                    run("ffmpeg","-v","error","-y","-analyzeduration","100000","-probesize","65536","-f","matroska",
                        "-i",f"tcp://127.0.0.1:{port}","-t","0.5","-map","0","-c","copy",str(output),timeout=12)
                    self.assertEqual([s["codec_type"] for s in probe(output)["streams"]],["video","audio"])
                    run("ffmpeg","-v","error","-xerror","-i",str(output),"-f","null","-")
                    if mode==2: self.assertEqual(first_frame(output)["color_transfer"],"smpte2084")
                finally:
                    process.terminate()
                    try: process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait(); self.fail("pipeline stop exceeded 3 seconds")
                    process.stderr.close()

    def test_invalid_port_is_rejected(self):
        for value in ("0", "1023", "65536", "abc", "9295junk"):
            result = subprocess.run([str(self.replay), "--fixture", str(BUILD / "fixtures/h264-opus.mkv"),
                                     "--port", value], capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)

    def test_occupied_port_is_not_silently_changed(self):
        with socket.socket() as reserved:
            reserved.bind(("127.0.0.1", 0))
            reserved.listen()
            result = subprocess.run([str(self.replay), "--fixture", str(BUILD / "fixtures/h264-opus.mkv"),
                                     "--port", str(reserved.getsockname()[1])], capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"bind/listen", result.stderr)

    def test_output_does_not_overwrite_existing_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "existing.mkv"
            output.write_bytes(b"keep this data")
            result = subprocess.run([str(self.replay), "--fixture", str(BUILD / "fixtures/h264-opus.mkv"),
                                     "--output", str(output)], capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), b"keep this data")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build/native")
    parser.add_argument("--case", choices=["remux-equivalence", "tcp", "synthetic-clock", "all"], default="all")
    args = parser.parse_args()
    BUILD = (ROOT / args.build_dir).resolve()
    names = unittest.defaultTestLoader.getTestCaseNames(RemuxTests)
    if args.case == "remux-equivalence":
        names = [n for n in names if n in ("test_nonseekable_remux_preserves_both_compressed_tracks",
                 "test_required_fixtures_have_video_audio_and_real_hdr", "test_output_does_not_overwrite_existing_file",
                 "test_live_annexb_metadata_without_decoder", "test_live_annexb_opus_remux",
                 "test_live_reordering_and_hdr_mismatch_are_rejected")]
    elif args.case == "tcp":
        names = [n for n in names if n in ("test_tcp_streams_both_tracks_with_fresh_headers_on_reconnect",
                 "test_invalid_port_is_rejected", "test_occupied_port_is_not_silently_changed",
                 "test_receiver_pipeline_delivers_both_tracks")]
    suite = unittest.TestSuite()
    if args.case in ("synthetic-clock", "all"):
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(ClockTests))
    if args.case != "synthetic-clock":
        suite.addTests(RemuxTests(n) for n in names)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(0 if result.wasSuccessful() and result.testsRun else 1)
