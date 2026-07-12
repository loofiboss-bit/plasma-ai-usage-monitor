#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

PROJECT=""
OUTPUT_DIR="${ROOT_DIR}/dist"
TAG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project)
      PROJECT="${2:-}"
      shift 2
      ;;
    --output-dir)
      OUTPUT_DIR="${2:-}"
      shift 2
      ;;
    --tag)
      TAG="${2:-}"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -z "$PROJECT" ]]; then
  echo "Usage: $0 --project <owner/project> --tag vX.Y.Z [--output-dir <dir>]" >&2
  exit 1
fi

if [[ -z "$TAG" ]]; then
  echo "COPR release builds require --tag vX.Y.Z" >&2
  exit 1
fi
bash "$ROOT_DIR/scripts/verify_exact_tag.sh" "$TAG"

if ! command -v copr-cli >/dev/null 2>&1; then
  echo "copr-cli is not installed. Install it first, then rerun this command." >&2
  exit 1
fi

if [[ ! -f "${HOME}/.config/copr" ]]; then
  echo "Missing COPR config at ${HOME}/.config/copr" >&2
  echo "Create an API token in COPR and save the config before submitting builds." >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"
bash "${ROOT_DIR}/scripts/build_srpm.sh" --output-dir "$OUTPUT_DIR" --tag "$TAG"

SRPM_PATH="$(find "$OUTPUT_DIR" -maxdepth 1 -name '*.src.rpm' | sort | tail -n 1)"

if [[ -z "$SRPM_PATH" ]]; then
  echo "Failed to locate generated SRPM in $OUTPUT_DIR" >&2
  exit 1
fi

copr-cli build "$PROJECT" "$SRPM_PATH"
