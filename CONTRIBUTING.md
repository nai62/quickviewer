# Contributing

QuickViewer is discontinued, but forks and successor projects are welcome; see
the note at the top of [README.md](README.md).

## Start here

The developer documentation index is [developer/README.md](developer/README.md).
It covers toolchain setup, building, testing, and the C++ lint hook.

## Working on a change

1. Enable the repository hooks once per clone: `git config core.hooksPath .githooks`.
2. Make a focused change, with tests where the behavior can be tested.
3. Build and run the relevant tests; on Windows this is `scripts\verify-windows.cmd debug`.
4. Commit and open a pull request.

The rules that automation and agents follow - including not bypassing commit
hooks and the verification contract - are in [AGENTS.md](AGENTS.md).

## Reporting issues

Report issues at https://github.com/kanryu/quickviewer/issues, including the
configuration of the QuickViewer installation.
