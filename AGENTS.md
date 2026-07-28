# Repository Guidelines

## Project Structure & Module Organization

ChiyanMap is a C++20 client mod for Minecraft Bedrock built on LeviLamina.

- `src/mod/` contains mod startup, teardown, and shared globals.
- `src/hooks/` contains game, DirectX 11, UI, and player hooks. Keep hook code defensive: game pointers and client state may be unavailable during transitions.
- `src/state/` owns persistent settings, map caches, waypoints, and background managers.
- `src/worldgen/` is pure, thread-safe seed-map logic. Do not pass game objects into this layer.
- `tests/worldgen/` contains standalone assertion-based tests.
- `bin/ChiyanMap/` is generated mod output and packaged assets. Do not edit generated DLL/PDB files.
- `.github/workflows/` defines the Windows build and release automation.

## Build, Test, and Development Commands

Use xmake from the repository root:

```powershell
xmake f -a x64 -m release -p windows --levilamina_version=26.10.14 -y
xmake build ChiyanMap
xmake f -a x64 -m debug -p windows --levilamina_version=26.20.4 -y
xmake build worldgen_tests
xmake run worldgen_tests
```

`ChiyanMap` builds and packages the client DLL under `bin/ChiyanMap/`. Run the test target for both supported SDK versions (`26.10.14` and `26.20.4`) when touching shared code, hooks, or build configuration.

## Coding Style & Naming Conventions

Use C++20, four-space indentation, braces on the same line as declarations, and `#pragma once` in headers. Match nearby code: `PascalCase` for types and functions, `camelCase` for local variables, and `g_` for globals. Keep headers self-sufficient and include standard headers explicitly. Prefer copied, value-only data at thread boundaries; retain game API access on the game thread.

No formatter or linter is configured. Compile with the existing `/W4` warnings enabled and avoid adding warning suppressions unless an SDK header requires one.

## Testing Guidelines

Tests use the standard `assert` facility, not a third-party framework. Name test functions `Test...` and keep them deterministic. Add fixtures for RNG/placement changes, negative coordinates, zero seeds, cache invalidation, and worker lifecycle behavior. `worldgen_tests` must remain independent of a running Minecraft client.

## Commit & Pull Request Guidelines

Recent commits use short Chinese imperative summaries, such as `修复本地玩家头像过滤` or `优化小地图优先加载与边缘刷新`. Keep each commit focused. PRs should describe player-visible behavior, list tested LeviLamina versions, link relevant issues, and include screenshots or video for UI/rendering changes. Do not commit build outputs or unrelated local changes.
