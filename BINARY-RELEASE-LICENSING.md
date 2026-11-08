# Binary release licensing audit — 8 November 2026

**Status: NOT CLEARED FOR BINARY DISTRIBUTION.** This document tracks rights, not legal advice or an additional licence.

## Exact linked build and obligations

| Component | Local link/source | Licence seen | Finding |
|---|---|---|---|
| MAME 0.106-derived emulation core | `mame106/`, compiled into `libmamecore.a` and `libmamecoreUNROLL.a` | Historical MAME 0.106 non-commercial licence | Permits free non-commercial redistribution with complete corresponding source, copyright and disclaimer notices. Not GPL-compatible. |
| Future Retro Fusion bare-metal frontend | `baremetal/`, `circle-kernel/` | FRF original contribution copyright, subject to third-party licences | FRF copyright can be asserted only for FRF-authored contributions; existing upstream notice remains. |
| Circle | Linked static Circle libraries from the Circle submodule of Circle-stdlib | GPL-3.0-or-later, per checked-in file headers and COPYING | **Blocker:** a combined executable cannot simply be distributed under conflicting GPLv3 and historical MAME terms. |
| Circle-stdlib integration | `libcirclenewlib.a` and Circle-stdlib build support | GPL-3.0 project licence and various library-specific terms | **Blocker:** requires precise permission/scope review in addition to Circle's licence. |
| Newlib C library | `libc.a`, `libm.a`, Circle-newlib | Multiple original component licences/possible exceptions | Build-time dependency; check the exact built components, source and exceptions if binary is distributed. |
| TinyUSB | `third_party/frf_tinyusb_mame/` | MIT | Original licence included and source retained. |
| zlib | `third_party/zlib/` | zlib licence | Licence copied, sources retained. |
| Expat | `third_party/libexpat/` | Expat/MIT-style | Old shared source preserved; the original shared directory lacked COPYING. The added historical licence text needs independent provenance confirmation. |
| Bundled third-party code under MAME | `mame106/` (including legacy driver and CPU code) | Per-file notices may vary | Do not claim universal FRF ownership or overwrite original notices. |

The exact project source revision and linked kernel build are recorded in [BUILD-VERIFICATION.md](BUILD-VERIFICATION.md). Project source availability does not itself grant permission to combine incompatible licenses.

## Upstream clarification requested

- Circle: https://github.com/rsta2/circle/issues/739
- Circle-stdlib: https://github.com/smuehlst/circle-stdlib/issues/61

These enquiries **are not grants of permission**. A reply that merely suggests an approach may not be a binding licence. The relevant rights holders must explicitly authorise a narrowly defined GPLv3 section 7 additional linking permission, or the build must be restructured to remove incompatible GPLv3-covered components. Rights of other contributors and applicable licences still require review.

See the [GNU GPL FAQ on GPL-incompatible libraries](https://www.gnu.org/licenses/gpl-faq.en.html#GPLIncompatibleLibs) and the [AdvanceMAME historical licence FAQ](https://www.advancemame.it/doc-faq).

## MAME trademark

The historical MAME software licence is not a trademark licence. [MAME legal information](https://www.mamedev.org/legal.html) and [MAME about/derivative-work guidance](https://www.mamedev.org/about.html) require permission to use the MAME name as part of a derivative project's name. Obtain the relevant permission or use distinctive FRF branding and only necessary descriptive, properly attributed references. Neither project naming nor use of a logo implies MAMEDev endorsement.

## Requirements before publishing `kernel8-32.img`

- [x] Historical MAME source licence and existing copyright notices preserved in public source tree.
- [x] FRF original-work attribution in `COPYRIGHT.md`.
- [x] Source-only GitHub repository, no game ROMs or prebuilt kernel.
- [x] Compiler/linker verification against ARM32/ Pi 3 sources (not hardware execution).
- [ ] Signed/traceable applicable additional permission from **all necessary GPLv3 copyright holders**, or completed non-GPL replacement and re-audit.
- [ ] Clarify title/wordmark permission or choose non-confusing project branding.
- [ ] Confirm Expat source version and precise licence; inventory all other linked runtime copyright notices.
- [ ] Bundle, or offer appropriately, all necessary corresponding source/build scripts and applicable licence copies for the **exact binary**.
- [ ] Boot and test the *identical* release binary on Raspberry Pi 3, then record checksum and release notes.
- [ ] Review terms with qualified legal counsel if there is uncertainty over permissions.

## Alternative technical route

If the linking exception cannot be obtained, port the Circle-dependent framebuffer, SD/FAT, timer, interrupt, scheduler and USB/system integration to a permissively licensed or independently written backend. TinyUSB MIT may be reusable, but it does not remove Circle-linked code automatically. This is a substantial engineering project, not a documentation edit.

A second possible route is an MAME core with modern GPL-compatible code lineage and licences, but the historical 0.106-derived components would require a file-by-file provenance and replacement audit. Merely changing a root LICENSE file or using a newer MAME label does **not** relicense older code.
