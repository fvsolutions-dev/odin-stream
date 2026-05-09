FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

# Build tools needed by scikit-build-core + nanobind:
#   - cmake / ninja           : configure + build the C++ extension
#   - gcc / g++ / clang       : compilers (CC/CXX default to clang via devcontainer.json)
#   - python3 + python3-dev   : Python headers used by `find_package(Python …)`
#   - git                     : FetchContent pulls nanobind_pyarrow from GitHub at configure time
#   - pkg-config              : occasionally needed by upstream cmake modules
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        gcc \
        g++ \
        clang \
        clang-format \
        clang-tidy \
        lldb \
        gdb \
        git \
        ca-certificates \
        curl \
        pkg-config \
        python3 \
        python3-dev \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

# uv (Python package/project manager) — newest tagged release. Recent enough
# to know about released Python 3.14 builds; if you want full reproducibility
# pin by sha256 digest instead.
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

USER vscode
RUN mkdir -p /home/vscode/.claude \
    && ln -sf /home/vscode/.claude/claude.json /home/vscode/.claude.json
ENV PATH="/home/vscode/.local/bin:${PATH}"
USER root

WORKDIR /workspaces/odin-stream
