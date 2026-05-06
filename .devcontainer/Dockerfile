FROM fedora:44

RUN dnf install -y --setopt=install_weak_deps=False --nodocs \
    bash \
    git \
    && dnf clean all
