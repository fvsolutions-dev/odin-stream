# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Fixed
- Wheel builds work again in a fresh environment. nanobind 3.x dropped `dict::operator[]`
  for integral keys, which broke `ParameterMapDescriptor(dict)` at compile time; since
  `[build-system]` did not pin nanobind, every isolated build (all of CI) failed while
  local builds against a pinned nanobind 2.6 kept working.
- The wheel workflow now actually runs. It triggered on `push` to `master`, but the
  default branch is `main`, and all other triggers were commented out. It now builds on
  push to `main`, on PRs targeting `main` from branches in this repo, on a published
  release, and on manual dispatch. Fork PRs are skipped by a job-level condition, but
  note that on a public repo the enforcing control is the "Fork pull request workflows
  from outside collaborators" repo setting, not that condition — `pull_request` runs the
  workflow file from the PR's own merge ref, so a fork PR can edit the condition out.
- `upload_all` is release-gated. It had no `if:` guard, so once the trigger was fixed
  every push to `main` would have published to PyPI.
- Linux wheels can build at all: cibuildwheel is on v4.2.0 (was v2.22, whose default
  `manylinux2014` image has no installable pyarrow, so Arrow was built from source).
- Windows wheels ship a type stub. The extension could not be imported standalone
  there — no RPATH equivalent, and pyarrow's Arrow C++ DLLs are not on any default
  search path — so nanobind's install-time stubgen failed, and it swallows the error.
  Stub generation moved to build time via `tools/gen_stub.py`, which preloads pyarrow.
- Published wheels are Release builds (`cmake.build-type` was `Debug`).
- Wheels cover every interpreter `requires-python` allows (3.10+); 3.10 and 3.11
  previously fell back to building from the sdist.

### Changed
- CPython 3.12+ now gets a single `abi3` wheel per platform instead of one wheel per
  minor version, via `wheel.py-api = "cp312"`. The `STABLE_ABI` flag in CMakeLists.txt
  was already producing a limited-API extension, but scikit-build-core still tagged it
  version-specific, so the wheel count never actually dropped. Builds per platform go
  from five to three (cp310, cp311, cp312-abi3). `abi3audit --strict` runs on every
  build and fails it if the extension reaches outside the 3.12 limited API.
- `ParameterMapDescriptor(dict)` takes `{parameter id: ParameterDescriptor}`. It
  previously accepted bare type descriptors and assigned them into its own `dict`
  argument instead of the member map, so it always produced an empty map; a wrong
  mapping now raises instead of silently yielding nothing.
- `pyarrow` is pinned to `>=25,<26`, matching what wheels are linked against. The
  extension links the Arrow C++ libraries inside the pyarrow wheel rather than
  vendoring a copy, so a wheel only loads against the Arrow soname it was built for.
  The previous open `>=19` range allowed installs that failed at import.

### Added
- `StreamProcessor.get_schema_progress()` reports, per parameter-set identifier, how much of
  a self-describing schema has been learned and what it is still waiting on
  (`missing_ordinals`, `awaiting_type_ids`, `blocked_reason`), plus how much DATA that cost
  (`buffered_data_packets` held for replay vs `unresolved_data_packets` dropped). Lets a
  consumer explain why a stream has not decoded yet without re-parsing the wire.

### Initial public release
- First public release of `odin-stream`.
