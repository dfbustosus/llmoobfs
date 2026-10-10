#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
docker build --platform linux/amd64 -t moobfs-m1-test -f vm/Dockerfile .
docker run --rm --platform linux/amd64 -v "$PWD:/work" moobfs-m1-test sh vm/in-container.sh
