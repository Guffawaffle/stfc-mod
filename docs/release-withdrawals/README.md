# Release Withdrawal Ledger

`release-withdrawals.jsonl` is the durable repo-local record for fork release
withdrawal actions.

The file is intentionally JSONL so emergency actions can append a single record
without rewriting history. Each record should include the affected tag, state,
reason, replacement tag, operator, timestamp, and original release metadata when
available.

Use `global.stfc-mod-private.release-withdrawal` or
`scripts/axf/release-withdrawal.ps1` to create records. Commit ledger updates
after executing a real withdrawal.
