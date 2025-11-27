#!/bin/bash
#
# Run pmtr tests in a Docker container (for macOS/Windows development)
#
# Usage: ./scripts/test-in-docker.sh [build_type]
#   build_type: Debug (default), Release, or Sanitize
#
set -e

BUILD_TYPE="${1:-Debug}"
IMAGE_NAME="pmtr-test"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Building test container..."
docker build -t "$IMAGE_NAME" -f - "$PROJECT_DIR" << 'DOCKERFILE'
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
DOCKERFILE

echo "Running tests with BUILD_TYPE=$BUILD_TYPE..."
docker run --rm -v "$PROJECT_DIR:/src:ro" -w /build "$IMAGE_NAME" bash -c "
    cmake /src -DCMAKE_BUILD_TYPE=$BUILD_TYPE && \
    cmake --build . --parallel && \
    ctest --output-on-failure
"

echo "Tests passed!"
