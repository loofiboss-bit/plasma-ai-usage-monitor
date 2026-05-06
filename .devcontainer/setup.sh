#!/usr/bin/env bash
set -euo pipefail

dnf install -y --setopt=install_weak_deps=False --nodocs \
    appstream \
    cmake \
    extra-cmake-modules \
    gcc-c++ \
    git \
    just \
    kf6-kcoreaddons-devel \
    kf6-ki18n-devel \
    kf6-knotifications-devel \
    kf6-kwallet-devel \
    libplasma-devel \
    libsecret-devel \
    ninja-build \
    openssl-devel \
    python3 \
    python3-PyQt6 \
    qt6-qtbase \
    qt6-qtbase-devel \
    qt6-qtbase-private-devel \
    qt6-qtdeclarative-devel \
    rpmlint

dnf clean all

just check || true
