# C++ lint and the pre-commit hook

C++ formatting is enforced by `scripts/lint-cpp.py`, which drives a pinned
`clang-format`. The tracked pre-commit hook runs the same script, so a commit
made from any client is formatted the same way CI formats it.

## How a git pre-commit hook works

- `pre-commit` is an executable that git runs before it records a commit. An
  exit status of zero continues the commit; any non-zero status aborts it.
- Hooks are not part of the working tree by default, so they are not cloned.
  This repository keeps them in `.githooks/` and points git at that directory
  with a local setting. Enable it once per clone:

  ```bash
  git config core.hooksPath .githooks
  ```

- Because the setting lives in `.git/config`, a fresh clone, a CI checkout, or a
  second machine has no hook until it is enabled again.
- `git commit --no-verify` skips hooks. Do not use it here unless the user
  explicitly asks for it; see `../AGENTS.md`.
- The hook is a normal git hook, so it runs for every committer, including
  agents and IDE commit buttons. There is no separate agent path.

## What this repository's hook does

`.githooks/pre-commit` returns immediately when the staged changes contain no
first-party C++ file. Otherwise it runs:

```bash
uv run --script scripts/lint-cpp.py --staged --fix
```

`uv` resolves `clang-format==23.1.1` from the inline metadata in
`scripts/lint-cpp.py`, so no separate clang-format installation or version
selection is required. The script warns when it is run with a different
clang-format found on `PATH`.

`--fix` formats the staged files in the working tree and stages the result, so
the commit records the formatted content instead of the raw staged diff. The
hook prints the files it rewrote. Consequences worth knowing:

- A commit can contain content that was not in the staged diff. When the exact
  committed content matters, confirm it with `git show` after committing.
- When the only staged change was a formatting violation, formatting removes
  the difference and git records an empty commit. This is accepted behavior;
  `git reset --hard HEAD~1` drops the commit if it is not wanted.
- If a staged C++ file also has unstaged changes, the hook aborts with exit 2
  instead of staging those unrelated edits. Stage or stash them and retry.
- The hook needs `uv` on `PATH`; without it, commits that touch first-party C++
  fail with exit 2.

## Lint scope

Only tracked first-party files under `apps/`, `components/`, and `tests/` are
checked. `FIRST_PARTY_PREFIXES` in `scripts/lint-cpp.py` defines that list, and
vendored trees are also excluded by `.clang-format-ignore`.

## Running the lint manually

Check the files changed from `HEAD` (tracked and untracked):

```bash
uv run --script scripts/lint-cpp.py
```

Check only the staged files:

```bash
uv run --script scripts/lint-cpp.py --staged
```

Apply fixes to the working tree:

```bash
uv run --script scripts/lint-cpp.py --fix
```

Check the complete tracked first-party scope:

```bash
uv run --script scripts/lint-cpp.py --all
```

## CI

`.github/workflows/cpp-lint.yml` runs `uv run --script scripts/lint-cpp.py
--all` on pull requests. The local hook covers only staged files, so files that
were already unformatted still fail in CI even when the hook passes the current
commit.

## Environment requirements

- Install [uv](https://docs.astral.sh/uv/). The first run downloads the pinned
  clang-format wheel, so it needs network access; later runs use the uv cache.
- `uv` needs a writable cache directory. In a sandboxed or agent environment
  where the default cache (`~/.cache/uv`, `~/Library/Caches/uv*`) is read-only,
  `uv run` fails with `Failed to initialize cache ... Operation not permitted`
  and the commit is aborted. Point `UV_CACHE_DIR` at a writable path or run the
  command with write access to the cache directory.
