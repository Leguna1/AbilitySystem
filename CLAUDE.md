## Conventions

- UE 5.7, module AbilitySystem (`ABILITYSYSTEM_API`). C++ for systems, Blueprints for content.
- Never declare a local or parameter that hides a class member (C4458/UHT shadowing).
- Forward-declare in headers; include the full header in the .cpp that uses the type.
- Comments explain why, in plain words; designer-facing UPROPERTY tooltips matter.
- Prefer data-driven setup (data assets, tags) over hardcoding.

## Building

- Build from the terminal and fix compile errors before reporting done:

  ```
  & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" AbilitySystemEditor Win64 Development -Project="D:\Unreal Projects\AbilitySystem\AbilitySystem.uproject" -WaitMutex
  ```
