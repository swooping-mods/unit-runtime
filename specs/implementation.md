# Moving Unit Runtime: behavior and implementation

Describes the exported implementation **0.2.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

Shared moving-unit services: eight providers share up to 2,048 stable unit slots, spatial/contact queries and a 20 Hz logical clock with at most four catch-up steps per frame. Providers register callbacks and publish/remove their own actors. The runtime dispatches in stable provider order and separates authoritative host simulation from guest snapshots. A nonce handshake, current crew membership, increasing sequence validation and bounded command/result retries authenticate session traffic.

## State and lifecycle

Registries, spatial cells, snapshots and command watermarks belong to the current world/session. Reliable commands/results are deduplicated per peer/provider; replaceable snapshots use the unreliable channel. Heartbeat/retry age uses unscaled time independently of scaled simulation. Authority change suspends until reentry. The runtime does not itself persist provider ownership or debit wallets; each provider owns its durable ledger.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/unit-runtime/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Provides `unit-runtime.units` | 1 | 80 bytes |
| Provides `unit-runtime.contacts` | 1 | 16 bytes |
| Provides `unit-runtime.sources` | 1 | 48 bytes |

## Implementation sources

- [mod.c](../source/mods/unit-runtime/mod.c)
- [service.h](../source/mods/unit-runtime/service.h)
- [spatial.h](../source/mods/unit-runtime/spatial.h)
- [registry.h](../source/mods/unit-runtime/registry.h)
- [contact.h](../source/mods/unit-runtime/contact.h)
- [commands.h](../source/mods/unit-runtime/commands.h)
- [clock.h](../source/mods/unit-runtime/clock.h)
- [sources.h](../source/mods/unit-runtime/sources.h)

## Verification boundary

No host migration, durable cross-process transaction or universal navigation implementation. The optional Sources interface supports native optimizer consumers without exposing private slots. Real co-op, latency and workload performance need matching native-game checks.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
