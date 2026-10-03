# Build System (Kiro)

This project is C11, built with CMake. The dependencies (raylib, cJSON, Mini-XML)
are fetched automatically via `FetchContent` during the configure step.

## Canonical toolchain

Build in Kiro with the **MSYS2 UCRT64 (gcc) + Ninja** toolchain. This is the only
toolchain currently installed on this machine and matches the existing `build/`
cache. The MSVC paths referenced in `.vscode/tasks.json` point at a Visual Studio
BuildTools install that is not present, so do not use them.

- Compiler: `C:/Tools/msys64/ucrt64/bin/cc.exe` (gcc 15.2.0)
- Generator: Ninja (`C:/Tools/msys64/ucrt64/bin/ninja.exe`)
- Debugger (if ever needed): `C:/Tools/msys64/ucrt64/bin/gdb.exe`
- Build output: `build/Debug/raylib-game/raylib-game.exe`

## Build commands

Use the CMake presets defined in `CMakePresets.json`:

```powershell
# Configure (first time, or after CMakeLists.txt changes)
cmake --preset debug

# Build
cmake --build --preset debug
```

If a bare preset invocation ever fails to find the compiler, prepend the UCRT64
bin directory to PATH for the session first:

```powershell
$env:PATH = "C:/Tools/msys64/ucrt64/bin;$env:PATH"
```

The presets already inject this PATH, so this fallback is rarely needed.

## Notes

- Do not run build/compile/link/CMake for basic code changes. Only build when
  specifically troubleshooting build/compile/link issues (see `AGENTS.md`).
- `.vscode/launch.json` and `.vscode/tasks.json` are kept for VS Code use; Kiro
  cannot run those tasks or the `cppvsdbg`/`cppdbg` launch configs. Build via the
  presets above instead.
