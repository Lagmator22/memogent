# Security model

## Threat model

Memogent runs in-process inside a host mobile app. Its threat model covers:

1. **Malicious apps on the same device.** On Android / iOS the OS sandbox
   already isolates storage; Memogent never writes outside the app container.
2. **Shoulder-surfing / physical attackers.** Optional SQLCipher build flag
   encrypts the on-disk state with an app-owned key (Keystore / Keychain).
3. **Compromised networks.** Irrelevant by default — Memogent has zero
   outbound connections unless the host app opts into an HTTP-backed LLM
   adapter. Only `localhost:11434` (Ollama) is ever used in that case.
4. **Data exfiltration after model upgrade.** The C ABI is opaque-handle
   based. A new core binary can read old state because the schema is
   versioned, but it cannot leak prior state unless the host explicitly
   reads and exports it.

## What Memogent stores

- Recent `AppEvent`s (app id, timestamp, optional hour-of-day).
- Predictor state (Markov tables / LSTM tensors).
- Cache content that the host passes in via `put()` — Memogent never
  interprets this payload, so the host controls what is stored.

## What Memogent never stores

- Raw user content, messages, photos, location coordinates.
- Device identifiers beyond what the host passes in.
- Network traces.

## Encryption at rest

Build with `-DMEM_WITH_SQLCIPHER=ON`. On Android the key should live in
`KeyStore`; on iOS in the `Keychain`. The C ABI will expose
`mem_orchestrator_set_db_key(ptr, bytes, n)` in v0.2.

## Kill switch

Call `orchestrator.reset()` to wipe every tier + WAL in a single
transaction. Wire it to your "forget me" UI.

## Audit log

Every admission and eviction decision is written to the telemetry buffer
with inputs and rationale. The host can export this as JSON for compliance
audits.

## Disclosure

Please file security reports at [github.com/Lagmator22/memogent/security](https://github.com/Lagmator22/memogent/security).
