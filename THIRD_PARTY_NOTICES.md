# Third-party components

## Chiaki-ng

Source: https://github.com/chiaki-ng/chiaki-ng

Pinned revision: `43728af2e639fecb8a7af1cd68c29820a40de973`.

The library is reused under its original AGPL-3.0 license terms and applicable
OpenSSL exception text, preserved in `LICENSES/`. Original source notices and
submodule licenses remain in the fetched source. The initial patch only respects
an explicit embedding project's deployment target; it does not change protocol
or licensing behavior.

The native PSN account-identification flow adapts URL/client configuration and
token/account-ID exchange behavior from `gui/include/psnaccountid.h`,
`gui/src/psnaccountid.cpp`, and `gui/src/psntoken.cpp` at the same pinned revision.
The original account-ID code credits grill2010's script. New native code retains
the project's AGPL terms, does not copy Qt settings/UI dependencies, and adds
strict redirect/state validation and sanitized error handling.

## Build/runtime dependencies

FFmpeg, OpenSSL, curl, nanopb, MUnit, gf-complete/Jerasure, json-c, miniupnpc,
libevent, protobuf and build tools retain their respective licenses. No binaries
are distributed at this stage. Before bundling or redistribution, enumerate the
actual linked libraries and FFmpeg build options and include their full notices.

New project code is licensed under AGPL-3.0-only. This notice is not a claim that
an arbitrary third-party binary configuration has passed redistribution review.
