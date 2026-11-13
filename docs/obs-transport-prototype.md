# OBS transport prototype (synthetic media only)

This is a development replay sender, not a PS5 connection. It remuxes compressed
video plus stereo Opus through the new native Matroska writer and localhost TCP
server. It opens no media decoder or encoder; FFmpeg is used separately by tests
to generate fixtures and validate the received result.

## Run

```sh
scripts/build-native.sh --config Debug --tests
build/python/bin/python3 Tests/Support/make_fixtures.py --out build/native/fixtures
build/native/bin/capture-replay --fixture build/native/fixtures/hevc-opus.mkv --loop --port 9295
```

The sender prints `READY 127.0.0.1:9295`. Stop with Ctrl-C. The feed is unencrypted
and unauthenticated but bound only to IPv4 loopback; trust other local processes.
Only one consumer is allowed. A reconnect may be rejected briefly until old-peer
closure is observed; clients should retry. Each accepted stream gets fresh
Matroska headers. Press Ctrl-C before restarting with another fixture or port.

Fixtures:
- `h264-opus.mkv`: H264 SDR with moving pattern and synchronized flashes/tone.
- `hevc-opus.mkv`: H265 SDR (the planned default).
- `hevc-hdr-opus.mkv`: H265 Main10, PQ/BT.2020, analytically generated neutral
  patches at 0/10/100/1000 nits and 1000-nit flashes. Mastering and content-light
  metadata are signaled. This is not an SDR pattern relabeled HDR, nor a complete
  HDR gamut/display-calibration suite.

All are 320x180/30 fps, ten seconds. They prove transport behavior, not PS5
1080p60 throughput, image quality, or long-run source clock reconstruction.
Fixture generation may encode; the replay sender only copies compressed packets.
For debugging, `--output NEWFILE.mkv` runs nonseekable file remuxing instead of
TCP. Existing files are never overwritten. `--loop`/`--port` cannot accompany it.

## OBS setup / manual Gate A

1. Add **Media Source**, turn **Local File** off.
2. Input: `tcp://127.0.0.1:9295`; Input Format: `matroska`; seekable off.
3. Reconnect delay: 1 second; close when inactive/restart when active: off.
4. Begin with minimum available network buffering; record actual settings.
5. Keep **Audio Monitoring Off** to avoid local speaker playback. Source audio
   must still be present in an OBS recording.
6. Verify picture and audible recorded tone/flash alignment. Restart the source
   and OBS; check fresh playback without old frames or growing A/V offset.
7. Repeat for H264, default H265 SDR, and H265 HDR. Stop the sender before changing
   fixtures. HDR requires a compatible decoder and appropriately configured OBS
   project/output (typically 10-bit and Rec.2100 PQ for these fixtures). Verify
   recorded HDR metadata and interpretation, not simply that an image appears.

Do not proceed past Gate A into independent login/pairing implementation until
the installed OBS build passes all required modes. FFmpeg receiver success does
not prove OBS compatibility. Record OBS version, project/output/decoder/buffering
settings, visual result, audio result, and reconnect behavior.

## Automated checks

Current manual result: the user confirmed all three feeds work in the installed
OBS setup. This accepts basic A/V ingestion compatibility for progressing to
account setup. It does not replace final sync measurement, calibrated HDR/output
checks, manual reconnect verification, or real PS5 testing.

```sh
scripts/test.sh --native --filter mux
scripts/test.sh --native --filter tcp
scripts/test.sh --native --app
```

The transport suite checks exact compressed packet hashes for these already
length-prefixed fixture packets, decodability in a separate validation process,
PQ/BT.2020/frame-level mastering and content-light metadata, numeric PQ patch
values, both tracks over TCP, ten reconnects per mode, rejection of a second
active consumer, port validation/conflicts, no output overwrite, bounded server
termination, and continuous timestamps across an eleven-second H265 replay.

Live PS5 Annex-B normalization, high-bitrate backpressure/queue limits, actual
OBS decoding, a thirty-minute synchronization/HDR soak, and source loss recovery
are NOT verified by these tests and remain later gates/tasks.
