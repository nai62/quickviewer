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

## Verification and handoff contract

Follow the repository `developer/Testing.md` runbook for commands and environment details.

- Do not run builds, automated tests, linters, benchmarks, deployment commands,
  or interactive checks unless the user explicitly requests their execution.
- This execution policy does not relax test-coverage requirements. Logic or
  file-loading changes require the relevant automated tests to be added or
  updated, and crash fixes require the regression test described above whenever
  the affected layer can be exercised deterministically.
- At handoff, separately report builds, automated tests, and interactive checks
  as not run when applicable, and provide the narrowest appropriate commands or
  manual checks from the runbook.
- For project/build-system or broadly shared changes, recommend a targeted
  compile and link appropriate to the change. Recommend complete Debug or
  Release verification only when the change warrants it or the user requests it.
- For startup painting, fullscreen, OpenGL, input timing, and other GUI behavior,
  provide the relevant automated-test command and the required interactive
  Windows checks.
- When the user requests verification, never hide warnings or failures and
  distinguish pre-existing failures from failures introduced by the current
  diff. Do not claim that all tests passed if an expected executable was missing
  or skipped. Use process exit codes and the current test output, not historical
  fixed test counts.
- Report the exact failing QtTest function and data row when available.

When task-specific user instructions conflict with this file, follow the user.
