# iPXE — Claude Code guidance

Agent conventions for this repository are tool-neutral and live in
[`AGENTS.md`](AGENTS.md) — follow them. This file adds only the Claude
Code-specific pieces.

- **Security review:** for vulnerability-hunting work, invoke the
  **ipxe-security-review** skill
  (`.claude/skills/ipxe-security-review/`). It encodes the sweep procedure
  and the terse reporting format described in `AGENTS.md`.
- **Codebase knowledge is at the source.** The bounds contracts of core
  helpers (`iob_*`, `asn1_cursor`, `xferbuf_*`, `ssnprintf`) and the
  coding patterns (composable cleanup, count-then-clamp) are documented in
  Doxygen documentation blocks at the source, indexed from the Notes
  section of `doc/threat_model.dox`. Treat those as authoritative; do not
  re-derive them.
