# Maintain dependencies and the developer bundle

1. Read `Dependencies.lock.json`. Keep Chiaki pinned and verify patch digests.
   The reference checkout is not an editable dependency or runtime requirement.
2. Edit fetched sources, export locked patches and update their SHA-256, then
   prove fresh application plus upstream/adapter regressions. Never leave fixes
   only in ignored build sources.
3. Preserve alternate-sink ordering, ownership, source-index/receipt semantics,
   recovery acceptance and legacy callback tests. Core struct layouts changed;
   no binary ABI compatibility with unpatched Chiaki is claimed.
4. Run native ASan/UBSan and supported TSAN separately from Swift/UI hosts.
   Include two-track remux, receiver pipeline and fault cases.
   The explicit direct unit runner supplies a minimal environment: invalid
   `xctest` invocations can print inherited environment variables in diagnostics.
   Never pass a secret-bearing environment to an exploratory runner invocation.
5. Review FFmpeg public parser/mux changes, payload equivalence, HDR metadata,
   no-reorder admission, live Opus pre-skip and bounded storage. Never open a
   decoder as a metadata workaround.
6. Current Homebrew libraries require macOS 26; project targets reflect that.
   Older macOS needs separately built/tested compatible dependencies. Homebrew
   formula revisions and binary contents are not a reproducible binary lock.
   FFmpeg runtime is now the exception: use the private pinned source builder
   and explicit CMake prefixes. Homebrew ffmpeg/ffprobe remain fixture tools only.
   Do not link the fixture-input profile into the app. Re-run the actual runtime
   registry policy test after dependency/profile changes; see [portability](portability.md).
7. Packaging recursively copies dylibs, fixes install names/removes external
   rpaths, includes formula notices and signs inside-out. Existing output is
   never overwritten; nothing installs into Applications. The bundle checker
   examines all Mach-O dependencies, architecture, minimum OS and signatures.
8. Review the bundle's `Contents/Resources/RuntimeNotices/runtime-manifest.json`,
   formula recipes and full license texts before redistribution. Resolve actual
   FFmpeg options, GPL/AGPL/OpenSSL compatibility and transitive notice/source
   obligations. Closure checks are not legal review or a fulfilled source offer.
   Follow [source-distribution requirements](source-distribution.md) and refresh
   the dependency inventory for each candidate. Include static curl's `COPYING`
   alongside the other embedded-source notices; a dylib-only inventory misses it.
9. Test on a clean account/machine without developer paths. Independent code,
   security and license review plus real PS5/OBS evidence remain release gates.
   Do not publish, notarize, enable autostart or install without authorization.
