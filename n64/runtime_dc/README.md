# Dreamcast runtime (KallistiOS)

Builds a Dreamcast **ELF** (and **CDI** if `mkdcdisc` is installed) from a Pyrite64 project.

## Requirements

1. [KallistiOS](https://github.com/KallistiOS/KallistiOS) with toolchain (`KOS_BASE`, `environ.sh`)
2. Optional: [mkdcdisc](https://gitlab.com/simulant/mkdcdisc) on `PATH` for `.cdi` output

## Editor

**Build → Build for Dreamcast** exports `filesystem/`, writes `build-dc/Makefile`, then runs:

```bash
source "$KOS_BASE/environ.sh"
make -C build-dc
```

Outputs:

- `build-dc/<rom>_dc.elf`
- `build-dc/<rom>_dc.cdi` (when mkdcdisc is available; disc data = project `filesystem/`)

## Runtime notes

- Reuses the PC engine host path (`PLATFORM_PC` + `PLATFORM_DC`): soft RDP framebuffer → RGB565 VRAM blit
- Assets: `rom:/p64/...` → `/cd/p64/...` (from CDI) or `/rd/p64/...` (romdisk)
- Maple controllers mapped to the libdragon joypad API
