#!/usr/bin/env bash
set -euo pipefail

print_usage() {
  cat <<EOF
Usage: \$0 [--prefix PATH]

Installs the Bazel-staged enblend-enfuse tree using rsync.
If --prefix is provided, contents of the staged install tree are mirrored into PATH.
If the destination is not writable, the script attempts to escalate with sudo.

If --prefix is omitted, the script prints the path to the staged install tree produced by
this Bazel build and exits.

Examples:
  bazel run //:install_tree -- --prefix=/usr/local
  bazel run //:install_tree -- --prefix "\$HOME/.local"
EOF
}

PREFIX=""
STAGED_TREE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      print_usage
      exit 0
      ;;
    --prefix)
      if [[ $# -lt 2 ]]; then
        echo "--prefix requires a path argument" >&2
        exit 2
      fi
      PREFIX="$2"
      shift 2
      ;;
    --prefix=*)
      PREFIX="${1#*=}"
      shift 1
      ;;
    --staged-tree)
      if [[ $# -lt 2 ]]; then
        echo "--staged-tree requires a path argument" >&2
        exit 2
      fi
      STAGED_TREE="$2"
      shift 2
      ;;
    --staged-tree=*)
      STAGED_TREE="${1#*=}"
      shift 1
      ;;
    *)
      echo "Unknown argument: $1" >&2
      print_usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "${STAGED_TREE}" ]]; then
  echo "Error: path to staged install tree not provided (expected via --staged-tree)." >&2
  exit 2
fi

if [[ -z "${PREFIX}" ]]; then
  echo "${STAGED_TREE}"
  exit 0
fi

if [[ ! -d "${STAGED_TREE}" ]]; then
  echo "Staged install tree not found: ${STAGED_TREE}" >&2
  exit 1
fi

NEED_SUDO=0
if [[ -d "${PREFIX}" ]]; then
  if [[ ! -w "${PREFIX}" ]]; then
    NEED_SUDO=1
  fi
else
  PARENT="$(dirname "${PREFIX}")"
  if [[ ! -d "${PARENT}" || ! -w "${PARENT}" ]]; then
    NEED_SUDO=1
  fi
fi

RSYNC_ARGS=(
  -a
  --no-owner
  --no-group
)

if [[ "${NEED_SUDO}" -eq 1 ]]; then
  echo "Destination '${PREFIX}' not writable; attempting sudo for installation..."
  if [[ ! -d "${PREFIX}" ]]; then
    sudo mkdir -p "${PREFIX}"
  fi
  sudo rsync "${RSYNC_ARGS[@]}" "${STAGED_TREE}/" "${PREFIX}/"
else
  mkdir -p "${PREFIX}"
  rsync "${RSYNC_ARGS[@]}" "${STAGED_TREE}/" "${PREFIX}/"
fi

echo "Installed to: ${PREFIX}"
