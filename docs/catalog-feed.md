# Signed provider catalog feed

The provider catalog is local-first and never scrapes a provider from the
widget. A signed feed is an optional, administrator-configured update path for
reviewed catalog snapshots.

## Trust contract

- The payload must be Catalog v7 with `runtimeScraping: false`.
- Ed25519 signatures are verified with the public key pinned in
  `plugin/catalogupdater.cpp`.
- The SHA-256 digest covers the compact UTF-8 JSON payload exactly as it is
  verified by the client.
- The sequence is strictly monotonic. Duplicate and rollback feeds are
  rejected.
- `expiresAt`, `hardExpiresAt`, and `minAppVersion` are checked before cache
  activation.
- The catalog and metadata use `QSaveFile`; a failed activation leaves the
  previous verified cache in place.
- HTTPS redirects remain inside the allowlist. The default public host is
  `raw.githubusercontent.com`; a deployment may add one explicit host through
  `AIUSAGE_MONITOR_CATALOG_FEED_HOST`.

The updater is inactive unless `AIUSAGE_MONITOR_CATALOG_FEED_URL` is set. The
protected workflow publishes the envelope to the `catalog-feed` branch. A
deployment can point the app at:

`https://raw.githubusercontent.com/loofiboss-bit/plasma-ai-usage-monitor/catalog-feed/catalog-feed/providers-v4.json`

## Maintainer workflow

1. Review and merge the catalog change on `main`.
2. Generate or rotate an Ed25519 PEM key outside the repository. Pin only its
   public key in a reviewed application release; never commit the private key.
3. Store the PEM as the protected `CATALOG_FEED_SIGNING_KEY` environment secret.
4. Run **Publish signed provider catalog feed** manually with the next
   sequence, an expiry, and the minimum compatible app version.
5. Confirm the public raw file and the in-app Catalog Trust panel before
   considering the update live.

The local signer is deterministic and credential-free apart from the caller's
OpenSSL key file:

```text
python3 scripts/sign_catalog_feed.py \
  --key /secure/path/catalog-ed25519.pem \
  --sequence 2 \
  --expires-at 2026-09-30T00:00:00Z \
  --min-app-version 19.0.0 \
  --output /secure/path/providers-v4.signed.json
```

The daily drift workflow checks structure, lifecycle, freshness, declared
official sources, and optional bounded HTTPS reachability. Its report keeps
all review items visible, but separates expected manual-review states from
actionable drift:

- lifecycle replacements and deliberately unavailable pricing remain expected
  review items; the application stays fail-closed and does not invent a price;
- stale source evidence, invalid catalog policy, and durable HTTP failures
  (for example 404/410) are actionable review items;
- transient network failures are recorded as expected warnings because a
  reachability timeout alone does not prove catalog drift.

The scheduled workflow uses
`python3 scripts/check_catalog_drift.py --network --strict-actionable-review`.
It opens or updates the maintainer issue only when actionable items or errors
are present. The audit never changes or activates pricing automatically.
