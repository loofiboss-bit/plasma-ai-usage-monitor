# GitHub, KDE Store and COPR submission checklist

Use this checklist for the current version in `VERSION`. For 25.0.0, the
qualification record is [the release checklist](../release/v25.0.0-checklist.md).

## Prepare the exact candidate

- Run `just build-debug`, `ctest --preset debug --output-on-failure`,
  `just check`, `just qml-lint` and `just release-check`.
- Confirm required CI checks pass for the final PR head before merging.
- Keep README, changelog, user guide, generated wiki, AppStream and RPM metadata
  aligned with the release. Generate the wiki with
  `python3 scripts/generate_wiki_docs.py`.
- Validate all 13 canonical synthetic screenshots with
  `python3 scripts/check_release_media.py`. Their source identity must match
  the shipped runtime. Keep physical desktop/accessibility limits explicit.
- Review [Store listing copy](v25.0.0-listing.md) and
  [release notes](../release/v25.0.0-notes.md).
- State that the Store archive contains the frontend and requires the matching
  compiled plugin. Fedora COPR provides both components.

## Publish

1. Merge the qualified PR into `main` and fetch the resulting commit.
2. Create an annotated `v25.0.0` tag on that exact `origin/main` commit.
   Run `bash scripts/verify_exact_tag.sh v25.0.0`, then push the tag.
3. Wait for the canonical tag-triggered Release workflow to succeed. Read back
   the published source tarball, plasmoid, SPDX source SBOM, SHA-256 checksums
   and GitHub build provenance before claiming GitHub delivery.
4. Run `just copr-submit v25.0.0` against the same tag. Verify every enabled
   Fedora target independently, including signed RPM metadata and repository
   publication. A submitted build is not a successful package publication.
5. Update [KDE Store product 2353976](https://store.kde.org/p/2353976) with the
   exact GitHub plasmoid, version, listing and changelog. Use five representative
   current captures: Overview, session history, source detail, provider settings
   and Budget Control. Reload and verify persistence and public download bytes.
6. Publish `docs/wiki/*.md` to the GitHub wiki from the release commit. Read back
   the public installation and getting-started guides.
7. Record exact tag/commit, workflow, asset hashes, COPR build/targets, Store
   version/artifact and wiki identity in a separate publication-evidence commit.
   Preserve the release tag.

## Public discovery

- README starts with the purpose, installation route and first successful use.
- GitHub repository description, homepage and topics identify the Plasma widget.
- GitHub release notes link to installation, user guide and troubleshooting.
- COPR description and installation instructions explain the complete package.
- KDE Store clearly links the native-plugin prerequisite and installation guide.
- Unavailable values, estimates and opt-in local collection remain explicit.

If a named surface cannot be verified, record its actual blocker and continue
independent publication work. The overall release is incomplete until all named
surfaces have direct public evidence.
