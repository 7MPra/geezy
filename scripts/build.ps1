$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    docker run --rm --mount "type=bind,source=$((Get-Location).Path),target=/project" -w /project devkitpro/devkitppc:latest bash -lc 'set -e; gcc -Wall -Wextra -Werror tests/state_test.c -lm -o /tmp/geezy-test; /tmp/geezy-test; make; mkdir -p apps/geezy; cp geezy.dol apps/geezy/boot.dol'
    if ($LASTEXITCODE -ne 0) { throw 'Geezy build failed' }
} finally { Pop-Location }
