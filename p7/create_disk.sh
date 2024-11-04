#!/bin/bash
set -euxo pipefail

rm -f disk
truncate -s 1M disk
