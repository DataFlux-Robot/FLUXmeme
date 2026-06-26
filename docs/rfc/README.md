# FLUXmeme RFCs

Format-level changes to FLUXmeme go through an **RFC** (Request for Comments)
process. The goal is a written, versioned record of *why* the format/semantics
changed, so implementers across vendors can stay interoperable.

## When an RFC is required

- Changes to the `.flux` wire layout or the `FileHeader` / `Record` /
  `CommitMarker` structure.
- Adding/removing/renaming a record field, layer, or canonical `kind`.
- Changes to composition semantics (LIVRPS arcs, merge rules, variants).
- A new transcode target or a change to a canonical facet's schema.

## When an RFC is NOT required

- Bug fixes, internal refactors, performance work, new demos/tests/docs.
- Adding a **non-canonical** `kind` (open by design).
- Pure application-layer tools built on the SDK.

## Process

1. Copy `0000-template.md` (below) to `NNNN-short-name.md` (`NNNN` = next free
   number) and fill it in.
2. Open a PR titled `RFC NNNN: …`. Discussion happens on the PR.
3. After review, the RFC is marked **Accepted** / **Deferred** / **Rejected**.
   Accepted RFCs are implemented in a follow-up PR that bumps the format version
   (add-only fields; removals follow a deprecation cycle).

## Template

```markdown
# RFC NNNN: <title>
- Start date: YYYY-MM-DD
- Status: Draft | Accepted | Deferred | Rejected

## Summary
## Motivation (problem, examples)
## Detailed design (wire layout / semantics / examples)
## Backward & forward compatibility
## Alternatives considered
## Open questions
```
