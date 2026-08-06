---
name: ipxe-security-review
description: >-
  Systematic memory-safety review of iPXE's attacker-facing parsers. Use
  when asked to hunt for vulnerabilities, audit a network-input parser, or
  review a FILE_SECBOOT(PERMITTED) file that handles DHCP / DNS / TFTP /
  HTTP / TLS / EAP / PeerDist data or downloaded images, for length,
  overflow, or underflow defects.
---

# iPXE security review

The working conventions — threat model, scope, reporting format, and the
codebase bounds contracts — are in [`AGENTS.md`](../../../AGENTS.md). Read
them first. This skill is the step-by-step sweep procedure that applies
them.

## The sweep loop

Work one file at a time. Stop as soon as you find a confirmed defect.

1. **Pick a target.** A `FILE_SECBOOT ( PERMITTED )` file that parses an
   attacker input (threat model in `AGENTS.md`). Prefer wire parsers with
   manual length / offset arithmetic.
2. **Find the entry point** — the `*_rx` / deliver / parse function where
   attacker bytes first arrive together with a length.
3. **Establish reachability** — confirm the value is attacker-controlled
   and the path is reachable from network input, not reached only
   through trusted input (operator-configured settings or the
   embedded/boot script).
4. **Trace the arithmetic.** Follow every length, offset, and count from
   the wire to its use. Flag:
   - `size_t` underflow — `a - b` where `a < b` yields ~`SIZE_MAX`
     (reversed operands, or a missing header / trailer term).
   - integer truncation — a length held in `int` / `unsigned int` then
     used as `size_t` (LP64: 64-bit `size_t`, 32-bit `int`).
   - unchecked `iob_pull` / `iob_unput` (no bounds check — see
     `include/ipxe/iobuf.h`).
5. **Verify against the real allocation.** Use the documented helper
   contracts (`AGENTS.md` → Codebase contracts). Account for iobuf
   head / tailroom and the guaranteed end-of-image NUL.
6. **Classify.** An actual reachable defect → report. A provably in-bounds
   boundary case → note privately and move on. Do not emit theoretical
   findings.
7. **Report, then stop.**

## Reporting

    ! path/file.c:NNN  <attacker input> → <bug class>

Follow with a few sentences: how it is reached, and the consequence.
Nothing more. Stop so the maintainer can patch — do not batch defects and
do not open PRs. Keep findings private until fixed.

## Do not flag (see `AGENTS.md` → Patterns not to misread)

Composable-cleanup unreachable cleanup; count-then-clamp logical lengths;
end-of-image NUL reads; compiler-elided VLAs.
