# iPXE — agent guidance

iPXE is a network bootloader that runs pre-OS, in ring 0 / UEFI boot
services, frequently in Secure-Boot-signed builds. A memory-safety defect
in code that parses attacker-supplied data is therefore a pre-boot
code-execution / Secure-Boot-bypass primitive, not merely a crash. Review
standards are correspondingly high.

This file is tool-neutral: any coding agent should follow it. Claude Code
users additionally have `CLAUDE.md` and the `ipxe-security-review` skill,
which point back here.

The authoritative description of the threat model, the `FILE_SECBOOT`
scope semantics, the ring-0 / Secure-Boot stakes, and the exclusions
(malicious hardware, USB, UEFI peers, Infiniband) lives in
`doc/threat_model.dox`. **Read it first.** The bounds contracts of the
core helpers live **at the source**, in Doxygen documentation blocks;
the Notes section of `doc/threat_model.dox` is the definitive index of
links to them. Both are authoritative; the notes below are operational
pointers for review work, not a second copy.

## Contributions must have a human owner

`AGENTS.md` governs all agent activity here, not just security review.
Do not open pull requests or issue reports autonomously or unattended;
anything submitted upstream must be understood, owned, and defensible by
a human contributor who has reviewed it. Per the "(Ab)use of AI" policy
in `CONTRIBUTING.md`, unsolicited or unreviewed AI-generated pull
requests and issue reports are not welcome and result in a ban. The one
sanctioned exception is a concise, verified security vulnerability
report filed via `SECURITY.md`.

## Threat model & scope

Authoritative: `doc/threat_model.dox`. Operational deltas for reviewers:

- Concrete attacker inputs to hunt: DHCP / DHCPv6, DNS, TFTP, HTTP(S)
  headers / bodies / redirects, TLS, 802.1X / EAP / EAPoL, PeerDist /
  PCCRC, and downloaded images (ELF / PNM / …). At the weaker edge levels
  the model describes: USB descriptors, inputs from consumers of the EFI
  protocols iPXE *produces* (SNP, PXE Base Code, USB I/O, download), and
  platform-firmware data iPXE consumes (UEFI `LoadOptions`, `Boot####`
  device paths, configuration tables).
- `FILE_SECBOOT ( PERMITTED )` is the priority surface. Unmarked files are
  **lower priority but still in scope** — not "out of scope". `FORBIDDEN`
  is excluded by policy, not by cleanliness.
- Check each *file's own* `FILE_SECBOOT` marker, not its directory's: a
  file outside a subsystem's directory can silently miss the marker its
  siblings carry (an ONC-RPC helper outside `net/oncrpc` did exactly this).

## Reporting conventions

- One line per finding:
  `! path/file.c:NNN  <attacker input> → <bug class>`
- `!` marks a high-confidence memory-safety defect.
- Report only **actual, reachable** defects. Do the rigorous verification
  privately; keep the emitted report terse. (Verbose, speculative,
  AI-generated reports are actively unwelcome.)
- **One defect at a time** — stop after each confirmed defect so the
  maintainer can patch it. Do not batch findings.
- **Keep findings private until fixed.** No public issue or PR may
  describe an unfixed defect. Report a confirmed defect through the
  process in `SECURITY.md` — as a GitHub Security Advisory, and an
  AI-generated report must be filed against `ipxe/aipxe`, not
  `ipxe/ipxe`. Published artefacts (commit messages, the threat-model
  doc) stay descriptive: omit historical exploitation specifics that
  carry only offensive value (e.g. a pre-fix corruption window), even
  for already-fixed defects.
- **The maintainer writes the patch.** Identify the *correct fixed
  behaviour* first: e.g. a helper named after a POSIX function owes the
  POSIX validation contract, so validation belongs in that callee.

## Verify before reporting

A sweep produces **candidates, not findings** — however a candidate is
generated (a manual pass, or a fan-out of sub-agents partitioned by
subsystem). Every candidate must be verified first-hand against the source
(field types, the specific guard present or absent, the reachable call
path) before it is reported. A confident sub-agent write-up is an input to
that verification, never a substitute. Report only what survives.

## Review procedure (per target file)

1. Confirm the file's attacker input (above) and note its `FILE_SECBOOT`
   status — `PERMITTED` is priority, unmarked is lower priority.
2. Find the wire entry point (the RX / deliver / parse function where
   attacker bytes first arrive together with a length).
3. Confirm the suspect value is attacker-controlled and the path is
   reachable from network input — not reached only through trusted
   input such as operator-configured settings or the embedded/boot
   script.
4. Trace every length / offset / count from the wire to its use. Watch for:
   - `size_t` underflow (`a - b` with `a < b` → ~`SIZE_MAX`);
   - additive overflow before a bounds check (`a + b > limit`, computed in
     `uint32_t` / `size_t`, wraps and passes — recurred across SRP/FCP,
     ELF program headers, and FIP descriptors; the correct idiom is the
     subtraction form the tree uses elsewhere:
     `if ( a > limit || b > limit - a )`);
   - 32→64-bit truncation (LP64: `int` / `unsigned int` are 32-bit,
     `size_t` / `long` 64-bit);
   - missing header / trailer accounting.
   (Signed shift into the sign bit — `1 << 31` — is now caught at build
   time by `-Wshift-overflow=2`; do not hand-hunt it.)
5. Verify each access against the *real* allocation, using the documented
   helper contracts — do not re-derive them.
6. Classify the finding against `doc/threat_model.dox` — reachable ≠
   interesting:
   - a defect exploitable only by code already at iPXE's privilege is
     uninteresting (a malformed payload handed to a ring-0 loader — `nbi`,
     `elf` — can do nothing it couldn't do by simply executing; treat as
     hardening);
   - a driver defect triggered only by DMA-capable hardware is out of
     model; a USB / EFI-peer / firmware-input defect is on the edge (fix
     defensively, but rank accordingly).
   State the classification in the report so the maintainer can prioritise.
7. Report terse; stop.

## After the fix: review the PR

The maintainer writes the patch and opens a PR; reviewing it is part of
the loop. Confirm the diff actually closes the finding (correct idiom,
ordering matched to sibling checks, no new NULL-deref or regression) **and**
that the commit message's reachability / impact claims are accurate —
inversions and typos have both slipped through and are worth catching.

## Adjacent passes (not memory-safety)

You may be asked for these; they use a different lens:

- **Correctness / UB over `PERMITTED` drivers** — malicious hardware is
  still out of model; check only correctness and undefined behaviour.
- **Missing little-endian conversions** (`cpu_to_leXX` / `leXX_to_cpu`),
  best found by intra-file inconsistency (a wire / table / register field
  converted in most sibling accesses but host-order in one). Almost always
  **latent**: iPXE's hardware targets are little-endian, and the only
  big-endian target (s390x) runs solely as a Linux executable with no
  hardware drivers — so a driver-side miss is a correctness blemish, not
  exploitable. Only core / firmware-table code reachable on s390x-linux
  (e.g. an on-disk or config-table magic compared in host order) can be
  live.

## Codebase contracts (authoritative docs live at the source)

Cross-referenced from the Notes section of `doc/threat_model.dox`. Use
them; do not re-derive:

- `include/ipxe/iobuf.h` — the `iob_*` accessors (`iob_pull`, `iob_unput`,
  …) are bare pointer arithmetic with **no** production bounds check. The
  caller must validate lengths against `iob_len()` first.
- `crypto/asn1.c` — `asn1_cursor` helpers are self-checking; walking a
  structure via the helper API is bounds-safe by construction (a parse
  error invalidates the cursor to zero length).
- `core/xferbuf.c` — `xferbuf_*` accumulation is length-checked
  (`ensure_size` before copy).
- `core/vsprintf.c` — `ssnprintf` / `vssnprintf` clamp a negative
  remaining size to zero, so `used += ssnprintf(buf+used, len-used, …)`
  is safe as `used` approaches `len`.
- `core/malloc.c` — allocators are safe against malicious sizes and return
  NULL on failure (including a zero-size request); callers must handle
  NULL.
- `include/errno.h` — the composable structured-`goto` cleanup pattern
  (the cleanup that undoes a *successful* operation sits *above* that
  operation's `err_*` label, so an unreachable cleanup statement
  immediately before the first label is deliberate future-proofing, not
  dead code).

## Patterns not to misread as bugs

- **Count-then-clamp** — a parser may return a logical length larger than
  it actually wrote, having clamped the write to the buffer size. This is
  intentional.
- **End-of-image read** — downloaded images carry a guaranteed trailing
  NUL past `image->len` (see `include/ipxe/image.h`); a one-byte read
  there is in-bounds.
- **Elided VLAs** — an attacker-sized local used only via `typeof` /
  `sizeof` may be elided by the compiler at `-Os` / `-O2` but allocated on
  the stack at `-O0`; do not conclude "unreachable" from one build.
