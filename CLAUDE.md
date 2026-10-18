# UNPAID: OVERTIME — Project rules

First-person office comedy-horror with cooperative tasks and a hidden saboteur. Unreal Engine 5.8, C++ module `UnpaidOvertime`, target PC / Steam.

Design source of truth: `Docs/UNPAID_OVERTIME_Diseno_de_Juego.md` (Spanish). Read sections 0.1 and 16 before changing rules.

## Working rules

- Replies to the user in Spanish. Code, comments, identifiers, UI strings and commit messages in English. UI strings go through `LOCTEXT`/`NSLOCTEXT` (ES/EN localization later).
- When the user says "haz todo", do the whole task end to end without stopping between parts.
- Conventional commits, no AI attribution. The user makes commits and pushes.
- Never read or modify `.env*` files.
- If something could not be verified (build, tests, editor behavior), say so explicitly.

## Architecture

Code defines systems, data defines content. Gameplay logic lives in C++; Blueprints only for visuals.

| Folder (`Source/UnpaidOvertime/`) | Contents |
|---|---|
| `Core/` | Pure logic, no actors: match config + validation, task model and generator, star rating, day clock, AI difficulty. Fully unit-tested. |
| `Gameplay/` | GameMode, GameState, PlayerState, first-person character, interaction component, stations, task manager, office builder. |
| `AI/` | AI employees and their controller (C++ state machine; State Tree later). |
| `UI/` | Canvas HUD and Slate match-setup screen (no binary UI assets). |
| `Tests/` | Automation tests `UnpaidOvertime.Core.*`. |
| `Variant_Horror/`, `Variant_Shooter/` | Template code. Do not modify. |

Key rules encoded in code (see design doc):

- 4 to 8 tasks per day **in total**, cooperative, split into sub-objectives. Credit goes to whoever completes each sub-objective.
- Stars by individual contribution: `fairShare = totalSubObjectives / aliveEmployees`; 3★ ≥ 100 %, 2★ ≥ 50 %, 1★ > 0 %, 0★ = nothing.
- Day phases: PreGame → Shift → Overtime (cap +50 %) → Evaluation.
- Company name is mandatory, max 24 chars, validated server-side. No default.
- AIs never read hidden information; they use the same task API as humans.
- Server-authoritative: tasks, stars, firings, votes, sabotage and roles are decided in GameMode/GameState, never in UI.

## Build, run, test

- Editor is often open with Live Coding: do not build the editor target then. Verify with the game target. From `C:\Program Files\Epic Games\UE_5.8\Engine\Source` (Build.bat breaks on the spaces in the path from Git Bash; XGE/IncrediBuild is unlicensed, so pass `-NoXGE`):
  `../Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe ../Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll UnpaidOvertime Win64 Development -Project="<path>/UnpaidOvertime.uproject" -WaitMutex -NoHotReload -NoXGE`
- Live Coding cannot add new UCLASSes: after adding classes, close the editor and build `UnpaidOvertimeEditor` (same command, editor target).
- Run the MVP without binary assets: `Scripts\RunMvp.bat` (empty map + `UOGameMode`). Console var `uo.ShiftSecondsOverride` shortens the shift for testing.
- Run tests (editor closed): `Scripts\RunTests.bat`.
- After adding `.cpp`/`.h` files, regenerate Visual Studio project files (right click on the `.uproject`).
- Blueprints, maps and materials are binary: never edit them as text.

## Environment gotchas

- Windows Controlled Folder Access protects `Documents`: `git`, Bash and PowerShell cannot create folders in the project unless they are allowed apps.
- MCP: the editor's `ModelContextProtocol` plugin is configured in `.mcp.json` (`http://127.0.0.1:8000/mcp`); it only works with the editor open.
