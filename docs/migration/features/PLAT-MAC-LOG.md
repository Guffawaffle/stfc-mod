# PLAT-MAC-LOG — ordinary fallback and selected log history

Contract v1, 2026-11-02. Frozen source
`1baa55fedf1758b19f843cc59707b8901a0b4998`, starting refactor
`f65fa9af1219ea1e5e9adbfc42202d93bafdb9f9`. Source `patches.cc:109–144`
rotates the selected path, append-opens it, and reuses that operation after
ordinary Mac fallback selection. Current startup is `src/mod/mod.cc::StartLogging`.
Exact local upstream/play/play-dev identities are in [PLAT-PATHS](PLAT-PATHS.md).

| ID | Preserved behavior | Adapter / verification |
| --- | --- | --- |
| L1 | Preferred path follows File routing, including log-first Mac migration and native Unicode filenames. | Existing File::Path and native path conversion retained; shared file opener receives the selected path. Production filesystem fixture uses an actual Unicode filename. |
| L2 | Previous session becomes .1, older .1–.4 become .2–.5; the previous .5 is removed. Rotate the path actually opened, including fallback. | File::OpenSessionLog owns one rotate/open operation used by each attempt. Preferred-only extraction independently fails fallback history before correction. Real spdlog/filesystem fixture checks preserved bytes and new session output. |
| L3 | Rotation failure is diagnostic; open for append so existing session bytes survive. | Preserve filesystem exception containment and accumulated diagnostics. Synthetic nonempty .5 directory forces failure and verifies append. No test changes real log permissions. |
| L4 | Ordinary Mac preferred-open failure may use working-directory log, then console if both fail. | StartLogging supplies the fallback only inside the existing Mac ordinary branch. Shared helper preserves both open diagnostics and a console-ready logger after two failures. |
| L5 | Explicit custom or admitted profile must not escape its selected namespace after an open failure. | Same HasCustomNames/Requested refusal remains at the caller; no fallback option propagates open failure. Controlled helper case verifies no alternative log is touched. |
| L6 | Console sink, debug/trace level, flush behavior and IL2CPP error reporting stay owned by startup. | StartLogging retains those operations after obtaining the file result. No new feature hook, logger framework or setting. |

The port rotated the preferred log before attempting either location. Its fallback
called basic_logger_mt directly, leaving fallback history unrotated. The narrow
shared opener factors existing file operations into the existing File owner and
then restores the source's rotate/open ordering for both attempted locations.

The Windows fixture executes the actual common opener over owned temporary files;
it does not run the Mac Foundation directory lookup, migration branch or native
StartLogging. Mac fallback policy is bound by source inspection; current native
Mac qualification remains separate, including Mcohall's pre-release testing.
See the [cohort](../FEATURE-COHORT-20261102-PATH-PERSISTENCE.md) for exact results.
