#!/bin/bash
#
# Run pmtr tests in a Docker container (for macOS/Windows development)
#
# Usage: ./scripts/test-in-docker.sh [build_type]
#   build_type: Coverage (default), Debug, Release, or Sanitize
#
set -e

BUILD_TYPE="${1:-Coverage}"
IMAGE_NAME="pmtr-test"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Building test container..."
docker build -t "$IMAGE_NAME" -f - "$PROJECT_DIR" << 'DOCKERFILE'
FROM alpine:3.22

RUN apk add --no-cache \
    build-base \
    cmake \
    lcov \
    bash \
    gzip

WORKDIR /src
DOCKERFILE

echo "Running tests with BUILD_TYPE=$BUILD_TYPE..."

if [ "$BUILD_TYPE" = "Coverage" ]; then
    docker run --rm -v "$PROJECT_DIR:/src:ro" -w /build "$IMAGE_NAME" bash -c "
        cmake /src -DCMAKE_BUILD_TYPE=Coverage && \
        cmake --build . --parallel && \
        ctest --output-on-failure && \
        echo '' && \
        echo '=== Coverage Report ===' && \
        lcov --capture --directory . --output-file coverage.info --ignore-errors source,gcov 2>/dev/null && \
        lcov --remove coverage.info '*/test_*' --output-file coverage.info --ignore-errors unused 2>/dev/null && \
        lcov --list coverage.info
    "
else
    docker run --rm -v "$PROJECT_DIR:/src:ro" -w /build "$IMAGE_NAME" bash -c "
        cmake /src -DCMAKE_BUILD_TYPE=$BUILD_TYPE && \
        cmake --build . --parallel && \
        ctest --output-on-failure
    "
fi

echo "Done!"
