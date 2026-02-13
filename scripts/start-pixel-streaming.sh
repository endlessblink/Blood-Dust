#!/bin/bash
# Start Pixel Streaming 2 for Blood & Dust
# Launches signaling server + standalone game with pixel streaming enabled
#
# Usage:
#   ./scripts/start-pixel-streaming.sh              # default 1280x720
#   ./scripts/start-pixel-streaming.sh 1920 1080    # custom resolution

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="${PROJECT_ROOT}/unreal/blood-and-dust/bood_and_dust/bood_and_dust.uproject"
UE_EDITOR="${PROJECT_ROOT}/unreal/engine/Engine/Binaries/Linux/UnrealEditor"
WEBSERVERS="${PROJECT_ROOT}/unreal/engine/Engine/Plugins/Media/PixelStreaming2/Resources/WebServers"
SIGNALLING_DIR="${WEBSERVERS}/SignallingWebServer"
SIGNALLING_SCRIPTS="${SIGNALLING_DIR}/platform_scripts/bash"

RESX="${1:-1280}"
RESY="${2:-720}"

SIGNALLING_PID=""
GAME_PID=""

cleanup() {
    echo ""
    echo "Shutting down..."
    if [[ -n "$GAME_PID" ]] && kill -0 "$GAME_PID" 2>/dev/null; then
        echo "Stopping game (PID $GAME_PID)..."
        kill "$GAME_PID" 2>/dev/null || true
        wait "$GAME_PID" 2>/dev/null || true
    fi
    if [[ -n "$SIGNALLING_PID" ]] && kill -0 "$SIGNALLING_PID" 2>/dev/null; then
        echo "Stopping signalling server (PID $SIGNALLING_PID)..."
        kill "$SIGNALLING_PID" 2>/dev/null || true
        wait "$SIGNALLING_PID" 2>/dev/null || true
    fi
    echo "Done."
}
trap cleanup EXIT INT TERM

# --- Build signalling server if needed ---
echo "=== Checking signalling server build ==="

# Add locally-installed node to PATH if start.sh downloaded it
if [[ -d "${SIGNALLING_SCRIPTS}/node/bin" ]]; then
    export PATH="${SIGNALLING_SCRIPTS}/node/bin:$PATH"
fi

# Build Common library
if [[ ! -d "${WEBSERVERS}/Common/dist" ]]; then
    echo "Building Common library..."
    pushd "${WEBSERVERS}/Common" > /dev/null
    npm run build:cjs
    popd > /dev/null
fi

# Build Signalling library
if [[ ! -d "${WEBSERVERS}/Signalling/dist" ]]; then
    echo "Building Signalling library..."
    pushd "${WEBSERVERS}/Signalling" > /dev/null
    npm run build:cjs
    popd > /dev/null
fi

# Build SignallingWebServer (wilbur)
if [[ ! -d "${SIGNALLING_DIR}/dist" ]]; then
    echo "Building SignallingWebServer..."
    pushd "${SIGNALLING_DIR}" > /dev/null
    npm run build
    popd > /dev/null
fi

echo "=== Signalling server build OK ==="

# --- Start signalling server ---
echo ""
echo "=== Starting signalling server (player port 8080, streamer port 8888) ==="
pushd "${SIGNALLING_DIR}" > /dev/null
node ./dist/index.js \
    --serve \
    --console_messages verbose \
    --log_config \
    --http_root ./www \
    --player_port 8080 \
    --streamer_port 8888 \
    &
SIGNALLING_PID=$!
popd > /dev/null

echo "Signalling server PID: $SIGNALLING_PID"

# Wait for signalling server to be ready
echo "Waiting for signalling server on port 8080..."
for i in $(seq 1 30); do
    if curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8080 2>/dev/null | grep -q "200"; then
        echo "Signalling server is ready!"
        break
    fi
    if ! kill -0 "$SIGNALLING_PID" 2>/dev/null; then
        echo "ERROR: Signalling server exited unexpectedly"
        exit 1
    fi
    sleep 1
done

# --- Launch standalone game with pixel streaming ---
echo ""
echo "=== Launching Blood & Dust (${RESX}x${RESY}) with Pixel Streaming ==="
"${UE_EDITOR}" "${UPROJECT}" \
    -game \
    -ForceRes -ResX="${RESX}" -ResY="${RESY}" \
    -PixelStreamingURL="ws://127.0.0.1:8888" \
    -PixelStreamingEncoderCodec=H264 \
    -AudioMixer \
    -Unattended \
    -log \
    &
GAME_PID=$!

echo "Game PID: $GAME_PID"
echo ""
echo "============================================"
echo "  Pixel Streaming is starting up!"
echo "  Player URL: http://127.0.0.1:8080"
echo "  Resolution: ${RESX}x${RESY}"
echo "  Press Ctrl+C to stop everything"
echo "============================================"
echo ""

# Wait for either process to exit
wait -n "$SIGNALLING_PID" "$GAME_PID" 2>/dev/null || true
echo "A process exited, cleaning up..."
