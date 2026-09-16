# Retired queue completion repair

**THIS WAS FIXED BY SCOPELY.** The old off-screen Kir'Shara combat-completion workaround is obsolete and has been removed.

Removed components: `action_queue_repair.cc`, its two detours, installation entry, diagnostics source, and the `control.kirshara_queue_repair` setting. Existing configuration files may retain that key; it no longer enables any behavior.

Faster Queue Recovery addresses a separate remaining race: a queued target disappears while its course request is outstanding, then the failed response arrives after native code has already removed that target. This can delay planning the next target until the watchdog runs. Retiring the combat-completion workaround does not retire that recovery.
