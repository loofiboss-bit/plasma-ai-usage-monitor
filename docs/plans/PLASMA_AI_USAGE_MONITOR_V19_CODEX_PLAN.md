# Plasma AI Usage Monitor v19.0.0 — Verified Cost Intelligence

| Target | `19.0.0` |
| --- | --- |

## Objective

Every displayed estimate must be current enough for its review policy,
reproducible from immutable catalog evidence, explicit about missing dimensions,
and unavailable rather than guessed when trust requirements are not met.

## Implementation contract

- Catalog v7 is the single shipped price book. Runtime code never scrapes vendor
  pricing pages and never falls back from an unknown currency or retired model.
- Cost Engine v2 uses exact decimal/scaled arithmetic, explicit currency rounding,
  exact model IDs and explicit aliases only. Prefix matching is forbidden.
- Pricing dimensions include input/output, cache read/write, context tier,
  modality, service tier, route, region, additive fees, unit, and lifecycle.
- Every estimate carries catalog version, price ID, effective interval, source
  fingerprint, selected dimensions, component breakdown, and availability state.
- Empty currency remains nullable. An expired catalog stops new estimates and
  leaves the last known good price book available for diagnostics only.
- Signed remote feeds use pinned Ed25519 verification, HTTPS/allow-list policy,
  sequence monotonicity, schema and expiry checks, and atomic cache activation.

## Phases

0. Freeze the clean `main` baseline, release contract, and public surfaces.
1. Implement Catalog v7 normalization, evidence metadata, exact aliases, and
   source freshness/lifecycle validation.
2. Implement Cost Engine v2 and route all token and unit estimates through it.
3. Add signed-feed verification, rollback protection, expiry handling, offline
   cache states, and atomic activation.
4. Add immutable provenance to runtime metrics, snapshots, observations, and
   exports; preserve actual billing data separately from estimates.
5. Add drift-check adapters/workflows with review-only candidates and no
   automatic price activation.
6. Add Source Detail “Why this estimate?”, catalog trust state, unknown-price,
   conflict, and retirement-watch UX in existing surfaces.
7. Run full local qualification, package artifacts, tag the exact verified commit,
   publish GitHub/COPR/KDE Store, and independently read back every surface.

## Release gates

- All catalog rows have official evidence or an explicit unavailable/manual-review
  state; no runtime estimate uses prefix matching or empty-to-USD coercion.
- All estimate-producing runtime paths use Cost Engine v2 and expose provenance.
- Unit, integration, QML, package, static, catalog, migration, and exact-tag gates
  are green on Fedora 44-compatible tooling.
- GitHub tag/release/assets/checksums/checks are read back, COPR build/install is
  read back, and KDE Store version/file publication is read back.
- Physical/manual gates (camera, login, accessibility, reboot, audio, or browser
  interaction) are reported as `unverified` unless directly exercised.
