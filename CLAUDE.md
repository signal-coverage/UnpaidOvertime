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
| `Audio/` | Audio director world subsystem (music, ambience moods, day-flow sounds, volume mix), footstep component, `UUOSoundSet` id table. Pure decisions live in `Core/UOAudioRules`. See `Docs/Audio.md`. |
| `UI/` | Slate menus (main, setup, settings, pause, evaluation) and HUD; shared look in `UOUIStyle` (no binary UI assets). |
| `Tests/` | Automation tests `UnpaidOvertime.Core.*`. |
| `Variant_Horror/`, `Variant_Shooter/` | Template code. Do not modify. |

Key rules encoded in code (see design doc):

- 4 to 8 tasks per day **in total**, cooperative, split into sub-objectives. Credit goes to whoever completes each sub-objective.
- Stars by individual contribution: `fairShare = totalSubObjectives / aliveEmployees`; 3★ ≥ 100 %, 2★ ≥ 50 %, 1★ > 0 %, 0★ = nothing.
- Day phases: PreGame → Shift → Overtime (cap +50 %) → Evaluation (killers' private fake rating → review → direct firings (+ failed probations) → promotions → defenses → cut vote/tiebreak → meeting: 60 s debate, 15 s live vote, rock-paper-scissors on a tie, saving throw → outcome) → next day in the same match. The evaluation runs in `UUOEvaluationDirector`; the UI only submits choices through `AUOPlayerController::SubmitEvaluationChoice`.
- Company name is mandatory, max 24 chars, validated server-side. No default.
- AIs never read hidden information; they use the same task API as humans.
- Server-authoritative: tasks, stars, firings, votes, sabotage and roles are decided in GameMode/GameState, never in UI.

## Build, run, test

- Editor is often open with Live Coding: do not build the editor target then. Verify with the game target. From `C:\Program Files\Epic Games\UE_5.8\Engine\Source` (Build.bat breaks on the spaces in the path from Git Bash; XGE/IncrediBuild is unlicensed, so pass `-NoXGE`):
  `../Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe ../Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll UnpaidOvertime Win64 Development -Project="<path>/UnpaidOvertime.uproject" -WaitMutex -NoHotReload -NoXGE`
- Live Coding cannot add new UCLASSes: after adding classes, close the editor and build `UnpaidOvertimeEditor` (same command, editor target).
- Run the game: `Scripts\RunMvp.bat` (Lvl_Office, main menu). Console var `uo.ShiftSecondsOverride` shortens the shift for testing. On a map without `UOStation.*` anchors the GameMode falls back to the procedural greybox office.
- Automated in-game check: `Scripts\RunShots.bat` (dev switch `-UOShots`; `-UOAutoStart=<Company>` skips the menus; both compiled out of Shipping) saves screenshots to `Saved\Screenshots\UOAuto\` and enables `uo.AudioDebug 1` (logs every sound played; grep `UOAudio`). From Git Bash set `MSYS_NO_PATHCONV=1` before passing `/Game/...` paths.
- Multi-day / evaluation check: `Scripts\RunEvalTour.bat [random|win|lose] [days] ["-UOAIDifficulty=Easy" "-UOTasks=4" ...]` (dev switches `-UOEvalTour`, `-UOAutoPolicy=`, `-UOAutoDays=`; quote extra args, cmd splits at `=`). A scripted human works the shift and answers every evaluation prompt; screenshots go to `Saved\Screenshots\UOAuto\Eval\`. Log greps: `Starting Day`, `Difficulty day`, `Boss question` (with the fairness rationale), `Boss answer`, `Direct firing`, `Cut vote`, `Match over`.
- Killer / meeting check (M4.1): `Scripts\RunM4Tour.bat [employee|killer|kill|fakeone|forgery|duelrigged|duelfair|takeover] [days] ["-UOEmployees=6" ...]` (dev switches `-UOM4=`, `-UORole=Killer|Employee`, `-UOKillerCanKill=1`, `-UOEmployees=`, `-UOKillers=`, `-UOFakeStars=`, `-UOKillerAnswersWrong=1`, `-UOMeetingPolicy=tie:<a>:<b>|expel:<who>|abstain`, `-UOSavingAnswer=right|wrong`); screenshots to `Saved\Screenshots\UOAuto\M4\`. Log greps: `Roles:`, `Sabotage:`, `Fake rating`, `ALWAYS ACCEPTED`, `Trace #` (with the server audit of who really did it), `AI evidence` (every evidence item an AI learns, with its source, incl. `Public+Knowledge`), `AI statement`, `AI vote`, `Meeting`, `Duel`, `Saving throw`, `Kill:`, `Dismissal:`. Rules and tuning: `Core/UOKillerRules.h` (`FUOKillerTuning`), meeting `Core/UOMeetingRules.h`, suspicion model `Core/UOInvestigation.h`. Balance targets are checked by the abstract simulation `Core/UOBalanceSim` (test `UnpaidOvertime.Core.Balance.MeetingSimulation`).
- Art/level pipeline: `Scripts\BuildArt.bat [textures,materials,meshes,audio,level]` (comma- or space-separated). `uo_level.py` also places the room volumes (`UORoomVolume` + `UORoom.<Id>`) and tags props (`UOProp/UOColor/UOText/UOShape`) that the boss questions count at runtime; rebuild the level after changing furniture. `audio` synthesizes every sound in pure Python (`uo_audio_synth.py`), imports it and places the level audio (`uo_audio.py`); code plays sounds by id (`UOSoundId::*`), never by asset path. `Scripts/Editor/uo_level.py` and `uo_materials.py` are the source of truth for Lvl_Office lighting, anchors and materials.
- Run tests (editor closed): `Scripts\RunTests.bat`.
- After adding `.cpp`/`.h` files, regenerate Visual Studio project files (right click on the `.uproject`).
- Blueprints, maps and materials are binary: never edit them as text.

## Environment gotchas

- Windows Controlled Folder Access protects `Documents`: `git`, Bash and PowerShell cannot create folders in the project unless they are allowed apps.
- MCP: the editor's `ModelContextProtocol` plugin is configured in `.mcp.json` (`http://127.0.0.1:8000/mcp`); it only works with the editor open.
