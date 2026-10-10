FROM debian:trixie-slim@sha256:a29215f6a35e51e22adffa17f89e9d2ef06214e64a2bad10d765c46aea49f11f
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    gcc make libc6-dev linux-image-6.12.111+deb13-amd64=6.12.111-1 \
    linux-headers-6.12.111+deb13-amd64=6.12.111-1 \
    busybox-static cpio kmod qemu-system-x86 python3 ca-certificates e2fsprogs \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
