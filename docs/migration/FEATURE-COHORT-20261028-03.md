# Shop, claims, HUD and confirmations

Source-preserving reassessment on `play-dev-ripper`, starting at
`266a688cab23f668e660a0b20fdee33af03e19ee`. Frozen source remains
`1baa55fedf1758b19f843cc59707b8901a0b4998`; historical PLAY is
`cad98ef7990dd66167eb64ea5dda525c1fe2c296`, upstream comparison
`e884efa7c6ff66d0776b4cd4d6b76ce193296be5`.

| Capability | Contract | Resulting boundary |
| --- | --- | --- |
| PLAY-CLAIM | [Claims](features/PLAY-CLAIM.md) | Preserve filters and session defaults; reject stale show/update episodes |
| PLAY-OFFER-DETAILS | [Cooldown offers](features/PLAY-OFFER-DETAILS.md) | Preserve the actual cooldown bit test and native details action |
| PLAY-HUD | [Mission HUD](features/PLAY-HUD.md) | Qualify the existing retained native refresh/restoration implementation |
| PLAY-CONFIRMATIONS | [Confirmations](features/PLAY-CONFIRMATIONS.md) | Preserve narrow callback identity and native fallback |

Complete source implementations, supporting bindings, registrations, defaults,
Settings consumers and teardown paths are the contract inputs. Inventory names
alone are not behavioral specifications. No general UI/persistence framework,
new hook group, automatic reward claim or altered purchase policy is introduced.

## Bounded corrections

The gift drawer previously stored only a compared raw controller pointer. An
older show/update callback could continue after native code hid or replaced the
landing. A weak current owner and per-show revision now identify the episode;
temporary strong custody and identity rechecks protect native calls. Opening is
still attempted only once per landing and only after the native context enables
it. There is no recurring scan or persistent strong scene root.

Daily Claim All retains the frozen once-per-session default and 300-attempt
bound. Registering the weak episode before the native show callback allows a
reentrant hide to invalidate it. Frame work roots the current controller/toggle,
distinguishes unavailable isOn from false, and rechecks identity after callbacks.
A cancelled view cannot acknowledge or click the old toggle. This changes no
faction selection, reward eligibility or claim-server action.

Existing HUD baseline restoration and native-first setup remain unchanged.
Cooldown offers and confirmation behavior are assessed and exercised, not
rewritten for architectural uniformity.

## Haven shortcut investigation

The existing ALT-H route and frozen implementation both check the Gateway
prerequisite and request the planetary section with no visitor context.
Native client273 section callers, return-home method and director activation
were inspected separately; method names were not treated as UI-completion proof.
The installed GameAssembly hash is
`a9ab2edbc143cfba3aff35665e129006f73853070dbff206884fda04a235ab94`.
Inspection artifacts are `haven-entry-native.txt` and
`haven-section-callers.txt` under the directory below. Native methods can have
split unwind segments; a first unwind extent is not necessarily the whole body.

Guff then confirmed ALT-H worked from Station and from another Haven, before
any source change or deployment in this pass: "All said, it works!" No Haven
code is changed on that evidence. The earlier missing-UI report is retained as
intermittent/unreproduced, not assigned an invented cause. New Escape/visitor
Back behavior remains parked. [Entry contract](features/PLAY-HAVEN-ENTRY.md).

## Qualification and continuation

The new `shop-claims-native` mode executes actual production consumers within
the existing controlled IL2CPP executable. It uses synthetic managed metadata,
objects and callbacks, not a second presentation/claims implementation. Existing
HUD policies, mod, navigation, Haven and AAI-host regressions remain included.

Evidence directory:
`D:/dev/stfc-workspace/artifacts/haven-entry-shop-hud-20261028/`.
The first run exposed a fixture constructor mismatch: it created a second
EventSystem metadata identity instead of reusing the host's existing class.
That setup is corrected rather than weakening the pointer-click assertion.
The settled-input run `tests-2.log` passed **418/418** Windows build/run/install
steps with Debug assertions. It includes actual drawer and toggle cancellation,
reentry and ownership cases; native-list faction filtering with unknown rows
retained; HUD replacement/baseline restoration; cooldown bit combinations; and
confirmation callback identity, disabled and exception fallbacks. Existing
Settings, presets, navigation, Haven and AAI-host regressions remain included.
The source-input manifest binds the log and fixture executable to their inputs.
Native build and player results follow once executed. Windows compilation is
not native UI, reward delivery or Mac proof.

No source push, upstream merge, store/config mutation or broader parity claim.
Direct tooling only; no Runner-native execution or implementation fanout.
