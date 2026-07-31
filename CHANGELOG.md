# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added
- `StreamProcessor.get_schema_progress()` reports, per parameter-set identifier, how much of
  a self-describing schema has been learned and what it is still waiting on
  (`missing_ordinals`, `awaiting_type_ids`, `blocked_reason`), plus how much DATA that cost
  (`buffered_data_packets` held for replay vs `unresolved_data_packets` dropped). Lets a
  consumer explain why a stream has not decoded yet without re-parsing the wire.

### Initial public release
- First public release of `odin-stream`.
