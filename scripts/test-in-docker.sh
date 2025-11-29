#!/bin/bash
#
# Run pmtr tests in a Docker container (for macOS/Windows development)
#
# Usage: ./scripts/test-in-docker.sh [options] [distro] [compiler] [build_type]
#
#   distro:     alpine (default), debian, ubuntu, fedora, rocky, amazonlinux, arch, gentoo
#   compiler:   gcc (default), clang
#   build_type: Coverage (default), Debug, Release, Sanitize
#
# Examples:
#   ./scripts/test-in-docker.sh                     # alpine + gcc + Coverage
#   ./scripts/test-in-docker.sh debian clang        # debian + clang + Coverage
#   ./scripts/test-in-docker.sh ubuntu gcc Debug    # ubuntu + gcc + Debug
#   ./scripts/test-in-docker.sh --list              # show supported platforms
#
set -e

show_help() {
    cat << 'EOF'
Usage: ./scripts/test-in-docker.sh [options] [distro] [compiler] [build_type]

Options:
  --help, -h    Show this help message
  --list        List all supported distro/compiler combinations

Arguments:
  distro        Linux distribution (default: alpine)
                Supported: alpine, debian, ubuntu, fedora, rocky, amazonlinux, arch, gentoo

  compiler      Compiler to use (default: gcc)
                Supported: gcc, clang

  build_type    CMake build type (default: Coverage)
                Supported: Coverage, Debug, Release, Sanitize

Examples:
  ./scripts/test-in-docker.sh                     # alpine + gcc + Coverage
  ./scripts/test-in-docker.sh debian clang        # debian + clang + Coverage
  ./scripts/test-in-docker.sh ubuntu gcc Debug    # ubuntu + gcc + Debug
  ./scripts/test-in-docker.sh fedora clang Sanitize
EOF
}

show_list() {
    cat << 'EOF'
Supported platform/compiler combinations:

  Distro          libc    Compilers       Base Image
  ──────────────────────────────────────────────────────────
  alpine          musl    gcc, clang      alpine:3.22
  debian          glibc   gcc, clang      debian:bookworm
  ubuntu          glibc   gcc, clang      ubuntu:24.04
  fedora          glibc   gcc, clang      fedora:41
  rocky           glibc   gcc, clang      rockylinux:9
  amazonlinux     glibc   gcc, clang      amazonlinux:2023
  arch            glibc   gcc, clang      archlinux:base
  gentoo          glibc   gcc             gentoo/stage3

Notes:
  - Alpine + gcc: Sanitize build not supported (musl), auto-switches to Debug
  - Gentoo: Only gcc tested (clang requires manual setup)
  - Coverage build uses lcov/gcov (GCC-based, may not work with clang)
EOF
}

# Parse options
case "${1:-}" in
    --help|-h)
        show_help
        exit 0
        ;;
    --list)
        show_list
        exit 0
        ;;
esac

DISTRO="${1:-alpine}"
COMPILER="${2:-gcc}"
BUILD_TYPE="${3:-Coverage}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Validate distro
case "$DISTRO" in
    alpine|debian|ubuntu|fedora|rocky|amazonlinux|arch|gentoo) ;;
    *)
        echo "Error: Unknown distro '$DISTRO'"
        echo "Supported: alpine, debian, ubuntu, fedora, rocky, amazonlinux, arch, gentoo"
        exit 1
        ;;
esac

# Validate compiler
case "$COMPILER" in
    gcc|clang) ;;
    *)
        echo "Error: Unknown compiler '$COMPILER'"
        echo "Supported: gcc, clang"
        exit 1
        ;;
esac


# Validate build type
case "$BUILD_TYPE" in
    Coverage|Debug|Release|Sanitize) ;;
    *)
        echo "Error: Unknown build type '$BUILD_TYPE'"
        echo "Supported: Coverage, Debug, Release, Sanitize"
        exit 1
        ;;
esac

# Coverage with clang requires different tooling
if [ "$BUILD_TYPE" = "Coverage" ] && [ "$COMPILER" = "clang" ]; then
    echo "Warning: Coverage build uses gcov/lcov which is GCC-based"
    echo "         Results may be incomplete with clang"
fi

# Gentoo with clang requires manual setup
if [ "$DISTRO" = "gentoo" ] && [ "$COMPILER" = "clang" ]; then
    echo "Warning: Gentoo + clang requires manual clang installation"
    echo "         This combination is not tested in CI"
fi

# Alpine gcc doesn't support sanitizers (musl libc limitation)
if [ "$DISTRO" = "alpine" ] && [ "$COMPILER" = "gcc" ] && [ "$BUILD_TYPE" = "Sanitize" ]; then
    echo "Warning: Alpine + gcc does not support sanitizers (musl libc limitation)"
    echo "         Switching to Debug build"
    BUILD_TYPE="Debug"
fi

IMAGE_NAME="pmtr-test-${DISTRO}-${COMPILER}"

# Generate Dockerfile based on distro
generate_dockerfile() {
    case "$DISTRO" in
        alpine)
            cat << EOF
FROM alpine:3.22
RUN apk add --no-cache \\
    build-base \\
    cmake \\
    lcov \\
    bash \\
    gzip \\
    $( [ "$COMPILER" = "clang" ] && echo "clang compiler-rt" )
EOF
            ;;
        debian)
            cat << EOF
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \\
    build-essential \\
    cmake \\
    lcov \\
    procps \\
    $( [ "$COMPILER" = "clang" ] && echo "clang libclang-rt-dev" ) \\
    && rm -rf /var/lib/apt/lists/*
EOF
            ;;
        ubuntu)
            cat << EOF
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \\
    build-essential \\
    cmake \\
    lcov \\
    procps \\
    $( [ "$COMPILER" = "clang" ] && echo "clang libclang-rt-dev" ) \\
    && rm -rf /var/lib/apt/lists/*
EOF
            ;;
        fedora)
            cat << EOF
FROM fedora:41
RUN dnf install -y \\
    gcc \\
    gcc-c++ \\
    cmake \\
    make \\
    lcov \\
    procps-ng \\
    $( [ "$COMPILER" = "clang" ] && echo "clang compiler-rt" || echo "libasan libubsan" ) \\
    && dnf clean all
EOF
            ;;
        rocky)
            cat << EOF
FROM rockylinux:9
RUN dnf install -y \\
    gcc \\
    gcc-c++ \\
    cmake \\
    make \\
    procps-ng \\
    $( [ "$COMPILER" = "clang" ] && echo "clang" || echo "libasan libubsan" ) \\
    && dnf install -y epel-release \\
    && dnf install -y lcov \\
    && dnf clean all
EOF
            ;;
        amazonlinux)
            cat << EOF
FROM amazonlinux:2023
RUN dnf install -y \\
    gcc \\
    gcc-c++ \\
    cmake \\
    make \\
    procps-ng \\
    tar gzip \\
    $( [ "$COMPILER" = "clang" ] && echo "clang" || echo "libasan libubsan" ) \\
    && dnf clean all
EOF
            ;;
        arch)
            cat << EOF
FROM archlinux:base
RUN pacman -Syu --noconfirm \\
    base-devel \\
    cmake \\
    lcov \\
    procps-ng \\
    $( [ "$COMPILER" = "clang" ] && echo "clang" )
EOF
            ;;
        gentoo)
            cat << EOF
FROM gentoo/stage3
RUN emerge --sync --quiet && \\
    emerge -q dev-build/cmake sys-process/procps
EOF
            ;;
    esac
    echo "WORKDIR /src"
}

# Set compiler environment variables for cmake
get_compiler_env() {
    if [ "$COMPILER" = "clang" ]; then
        echo "CC=clang CXX=clang++"
    else
        echo "CC=gcc CXX=g++"
    fi
}

echo "========================================"
echo "Platform: $DISTRO"
echo "Compiler: $COMPILER"
echo "Build:    $BUILD_TYPE"
echo "========================================"
echo ""

echo "Building test container ($IMAGE_NAME)..."
generate_dockerfile | docker build -t "$IMAGE_NAME" -f - "$PROJECT_DIR"

echo ""
echo "Running tests..."

COMPILER_ENV=$(get_compiler_env)

if [ "$BUILD_TYPE" = "Coverage" ]; then
    docker run --rm -v "$PROJECT_DIR:/src:ro" -w /build "$IMAGE_NAME" bash -c "
        export $COMPILER_ENV && \
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
        export $COMPILER_ENV && \
        cmake /src -DCMAKE_BUILD_TYPE=$BUILD_TYPE && \
        cmake --build . --parallel && \
        ctest --output-on-failure
    "
fi

echo ""
echo "Done! ($DISTRO + $COMPILER + $BUILD_TYPE)"
