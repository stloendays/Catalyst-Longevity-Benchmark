#!/usr/bin/env bash
set -euo pipefail

REPO_URL="${REPO_URL:-https://github.com/stloendays/Catalyst-Longevity-Benchmark}"
RUNNER_NAME="${RUNNER_NAME:-tencent-rhocodec-4c4g}"
RUNNER_LABELS="${RUNNER_LABELS:-rhocodec,tencent,4c4g}"
RUNNER_DIR="${RUNNER_DIR:-/opt/actions-runner}"
RUNNER_USER="${RUNNER_USER:-ubuntu}"

if [ "$(id -u)" -ne 0 ]; then
  exec sudo -E bash "$0" "$@"
fi

if [ -z "${RUNNER_TOKEN:-}" ]; then
  printf 'GitHub runner registration token: ' >&2
  IFS= read -r -s RUNNER_TOKEN
  printf '\n' >&2
fi

if [ -z "$RUNNER_TOKEN" ]; then
  echo 'Registration token is required.' >&2
  exit 2
fi

if ! id "$RUNNER_USER" >/dev/null 2>&1; then
  echo "Runner user does not exist: $RUNNER_USER" >&2
  exit 2
fi

case "$(uname -m)" in
  x86_64|amd64) ;;
  *) echo "This bootstrap currently expects x86_64; found $(uname -m)." >&2; exit 2 ;;
esac

for cmd in curl tar python3; do
  command -v "$cmd" >/dev/null 2>&1 || { echo "Missing required command: $cmd" >&2; exit 2; }
done

mkdir -p "$RUNNER_DIR"
chown -R "$RUNNER_USER:$RUNNER_USER" "$RUNNER_DIR"

if [ -f "$RUNNER_DIR/.runner" ]; then
  echo 'A GitHub Actions runner is already configured in:' "$RUNNER_DIR"
  if ls "$RUNNER_DIR"/svc.sh >/dev/null 2>&1; then
    (cd "$RUNNER_DIR" && ./svc.sh status || true)
  fi
  echo 'Leaving the existing registration unchanged.'
  exit 0
fi

TMP_JSON=$(mktemp)
TMP_TGZ=$(mktemp --suffix=.tar.gz)
trap 'rm -f "$TMP_JSON" "$TMP_TGZ"; unset RUNNER_TOKEN' EXIT

curl -fsSL --retry 5 --retry-all-errors --connect-timeout 20 \
  https://api.github.com/repos/actions/runner/releases/latest \
  -o "$TMP_JSON"

RUNNER_URL=$(python3 - "$TMP_JSON" <<'PY'
import json, sys
with open(sys.argv[1], encoding='utf-8') as f:
    release = json.load(f)
for asset in release.get('assets', []):
    name = asset.get('name', '')
    if name.startswith('actions-runner-linux-x64-') and name.endswith('.tar.gz'):
        print(asset['browser_download_url'])
        break
else:
    raise SystemExit('Could not locate Linux x64 runner asset in latest release')
PY
)

echo "Downloading official GitHub Actions runner: $RUNNER_URL"
curl -fL --retry 5 --retry-all-errors --connect-timeout 30 \
  "$RUNNER_URL" -o "$TMP_TGZ"

tar -xzf "$TMP_TGZ" -C "$RUNNER_DIR"
chown -R "$RUNNER_USER:$RUNNER_USER" "$RUNNER_DIR"

if [ -x "$RUNNER_DIR/bin/installdependencies.sh" ]; then
  "$RUNNER_DIR/bin/installdependencies.sh" || true
fi

cd "$RUNNER_DIR"
sudo -u "$RUNNER_USER" -H ./config.sh \
  --url "$REPO_URL" \
  --token "$RUNNER_TOKEN" \
  --name "$RUNNER_NAME" \
  --labels "$RUNNER_LABELS" \
  --work _work \
  --unattended \
  --replace

./svc.sh install "$RUNNER_USER"
./svc.sh start
sleep 2
./svc.sh status || true

echo
echo 'Self-hosted runner bootstrap complete.'
echo "Repository: $REPO_URL"
echo "Runner:     $RUNNER_NAME"
echo "Labels:     self-hosted, Linux, X64, $RUNNER_LABELS"
echo 'The registration token was not written to disk by this script.'
