# ADR: Modeless GUI for Script Runner

- Status: Accepted
- Date: 2026-06-16
- Context:
  The Lua Script Runner dialog was a `DG::ModalDialog`.
  Modal dialogs block all ArchiCAD interaction while open.
  Users need to keep the dialog open while selecting elements in the plan,
  inspecting their properties, and iterating on scripts.
- Decision:
  **Change the dialog from `DG::ModalDialog` to `DG::Palette` (modeless).**

  Alternatives considered:

  1. **Keep `DG::ModalDialog`** — rejected.
     Modal blocks plan interaction.
     User would have to close/reopen the dialog on every edit iteration,
     breaking the workflow.

  2. **`DG::ModelessDialog`** — rejected.
     Available but behaves as a regular document window (can be buried behind
     other windows, no float-on-top behavior).
     A `Palette` stays above the plan, which is the expected UX for a tool window.

  3. **`DG::Palette`** — chosen.
     Floats above the main window.
     User can select elements, inspect them via the Lua Console, edit scripts,
     and re-run — all while the dialog stays open.
     Self-deletes on close via `PanelClosed` handler.
     Script execution remains synchronous (blocker mode) when Run is clicked.

- Pro/Con Analysis:
  - **`DG::Palette`**:
    Pro: floats above plan, allows concurrent ArchiCAD interaction,
    standard ArchiCAD pattern for tool windows.
    Con: requires heap allocation and self-delete lifecycle management.
    Con: multiple instances possible if menu clicked repeatedly (acceptable —
    each instance is independent).
  - **`DG::ModalDialog`** (keep current):
    Pro: simple stack allocation, no lifecycle concerns.
    Con: blocks all interaction, poor UX for iterative script development.

- Exit Strategy:
  If palette lifecycle (self-delete) causes stability issues, switch to a
  singleton palette managed by `Bridge` with show/hide semantics.
