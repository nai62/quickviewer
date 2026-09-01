# QuickViewer agent instructions

## Scope and working principles

These instructions apply to the entire repository.

- Keep user changes intact and make narrowly scoped edits.
- Do not commit generated build products, deployed Qt DLLs, or local test
  reports.
- Preserve observable behavior during refactoring unless a behavior change is
  explicitly requested.
- Prefer ownership and state invariants that are explicit in types. Do not
  replace a raw pointer with a smart pointer until its actual ownership and
  QObject thread affinity are understood.
- Add a regression test before or with a crash fix whenever the affected layer
  can be exercised deterministically.
- Never hide warnings or test failures. Distinguish pre-existing failures from
  failures introduced by the current diff.

## Verification contract

Follow the repository `TESTING.md` runbook for commands and environment details.

- Logic or file-loading changes require the complete automated test suite.
- Project/build-system or broadly shared changes also require a Release compile
  and link.
- Startup painting, fullscreen, OpenGL, input timing, and other GUI behavior
  require relevant automated tests plus explicit interactive Windows checks.
  If those checks were not performed, report them as remaining manual work.
- Do not claim that all tests passed if an expected executable was missing or
  skipped. Use process exit codes and the current test output, not historical
  fixed test counts.
- Report the exact failing QtTest function and data row when available.

When task-specific user instructions conflict with this file, follow the user.
