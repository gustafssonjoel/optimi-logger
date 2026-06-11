# Optimi Logger Requirement Traceability Matrix

This project is in requirement phase. Requirement catalogs are definition-only and are not expected to execute assertions yet.

## Ownership Rules

- `test_optimi_logger_acceptance.c` owns API lifecycle and end-to-end behaviors.
- `test_optimi_logger_config.c` owns configuration validation and sink/queue/filename configuration semantics.
- `test_optimi_logger_flushing.c` owns explicit flush semantics and flush lifecycle edge cases.
- `test_optimi_logger_header.c` owns header/API shape and macro forwarding semantics.
- `test_optimi_logger_threading.c` and `test_optimi_logger_memory.c` are smoke-style runnable scaffolds only.

## Matrix

| ID | Owner | Overlap | Conflict | Notes |
| --- | --- | --- | --- | --- |
| LOGGER-001..004 | acceptance | None | No | Core create/destroy/null contracts. |
| LOGGER-010..012 | acceptance | CONFIG-050 | No | Runtime filtering ownership stays in acceptance; config owns creation-time invalid level handling. |
| LOGGER-020..023 | acceptance | header | No | Header suite is canonical for macro forwarding mechanics; acceptance keeps behavior-level intent. |
| LOGGER-030..032 | acceptance | CONFIG-001..003, CONFIG-023 | No | Acceptance owns behavior-level overflow outcomes; config owns policy wiring at creation/config level. |
| LOGGER-040..043 | acceptance | FLUSH-002..003 | Resolved | Post-shutdown contract is NOT_INITIALIZED for log/flush/set_min_level. |
| LOGGER-050..052 | acceptance | CONFIG-010..015, CONFIG-054 | No | Acceptance keeps sink behavior and error propagation intent; config keeps switch/path parameterization. |
| LOGGER-060..061 | acceptance | None | No | Status-string mapping and fallback ownership. |
| CONFIG-001..003 | config | LOGGER-030..032 | No | Queue overflow policy behavior under explicit configuration. |
| CONFIG-010..015 | config | LOGGER-050..051 | No | Sink enablement and color behavior under config combinations. |
| CONFIG-020..023 | config | memory | No | Queue size contracts; memory suite may stress but does not own requirement definitions. |
| CONFIG-030..032 | config | FLUSH-004..007 | No | Config owns flush-mode parameter semantics; flushing owns explicit flush API semantics. |
| CONFIG-040..045 | config | None | No | Output path and filename generation behavior. |
| CONFIG-050..055 | config | LOGGER-052 | Resolved | Invalid enum/path outcomes are explicit, not "clamped or rejected" or "similar status". |
| FLUSH-001..003 | flushing | LOGGER-043 | Resolved | Flush/shutdown input and post-shutdown lifecycle behavior aligned. |
| FLUSH-004..007 | flushing | CONFIG-030..032 | No | Explicit flush behavior and idempotence coverage. |

## Resolved Requirement Ambiguities

- Invalid `min_level` at create time is rejected with `OPTIMI_STATUS_INVALID_ARGUMENT`.
- Inaccessible file output path is rejected with `OPTIMI_STATUS_IO_ERROR`.
- Invalid `overflow_policy` at create time is rejected with `OPTIMI_STATUS_INVALID_ARGUMENT`.
- After shutdown, `optimi_logger_log`, `optimi_logger_logv`, `optimi_logger_flush`, and `optimi_logger_set_min_level` return `OPTIMI_STATUS_NOT_INITIALIZED`.
