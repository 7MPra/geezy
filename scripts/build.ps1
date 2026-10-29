param([switch]$Profile)
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    docker run --rm --mount "type=bind,source=$((Get-Location).Path),target=/project" -w /project devkitpro/devkitppc:latest bash -lc 'set -e; gcc -Wall -Wextra -Werror tests/state_test.c -lm -o /tmp/geezy-test; /tmp/geezy-test; gcc -Wall -Wextra -Werror tests/midi_test.c -lm -o /tmp/geezy-midi-test; /tmp/geezy-midi-test; gcc -Wall -Wextra -Werror tests/world_test.c -lm -o /tmp/geezy-world-test; /tmp/geezy-world-test; gcc -Wall -Wextra -Werror tests/render_test.c -lm -o /tmp/geezy-render-test; /tmp/geezy-render-test; gcc -Wall -Wextra -Werror tests/master_test.c -lm -o /tmp/geezy-master-test; /tmp/geezy-master-test; gcc -Wall -Wextra -Werror tests/hybrid_test.c -lm -o /tmp/geezy-hybrid-test; /tmp/geezy-hybrid-test; make; mkdir -p apps/geezy; cp geezy.dol apps/geezy/boot.dol'
    if ($LASTEXITCODE -ne 0) { throw 'Geezy build failed' }
    if($Profile) {
        docker run --rm --mount "type=bind,source=$((Get-Location).Path),target=/project" -w /project devkitpro/devkitppc:latest bash -lc '/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -O2 -Wall -Wextra -DGEKKO -DGEEZY_PROFILE -mrvl -mcpu=750 -meabi -mhard-float -I/opt/devkitpro/libogc/include source/main.c -L/opt/devkitpro/libogc/lib/wii -lwiiuse -lbte -logc -lm -o geezy-profile.elf && /opt/devkitpro/tools/bin/elf2dol geezy-profile.elf geezy-profile.dol'
        if ($LASTEXITCODE -ne 0) { throw 'Geezy profiling build failed' }
    }
} finally { Pop-Location }
