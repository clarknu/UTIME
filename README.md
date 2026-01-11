# UTIME - Windows IME Framework

This is a minimal Windows Input Method Editor (IME) framework using the Text Services Framework (TSF).

## Prerequisites

- **Visual Studio 2019/2022** with "Desktop development with C++" workload installed.
- **Windows SDK** (usually included with Visual Studio).

## Project Structure

- `src/`: Source code files.
    - `TextService.cpp`: Core IME logic (Key handling, Composition).
    - `EditSession.cpp`: Helper for modifying text in the application.
    - `Register.cpp`: DLL registration logic.
    - `dllmain.cpp`: DLL entry point.
- `include/`: Header files.
- `UTIME.sln`: **Visual Studio Solution File** (Double-click to open).
- `UTIME.vcxproj`: Project file.

## Features Implemented

- **Basic Typing**: Intercepts 'A'-'Z' keys.
- **Composition**: Displays underlined text as you type.
- **Hardcoded Dictionary**:
    - Type `nihao` + Space -> Output `你好`
    - Type `ceshi` + Space -> Output `测试`
    - Type `utime` + Space -> Output `U时间`
    - Other text + Space -> Output original text
- **Editing**: Supports Backspace and Escape.

## How to Build (Visual Studio)

1. Double-click `UTIME.sln` to open the project in Visual Studio.
2. Select the build configuration (e.g., **Debug** or **Release**) and platform (**x64** is recommended).
3. Right-click the project `UTIME` in Solution Explorer and select **Build**.
4. The output DLL will be in `bin/x64/Debug/UTIME.dll` (or similar depending on config).

## How to Install/Register

1. Open a Command Prompt **as Administrator**.
2. Navigate to the build output directory (e.g., `bin/x64/Debug`).
3. Run:
   ```cmd
   regsvr32 UTIME.dll
   ```

## How to Use

1. Go to Windows Settings -> **Time & Language** -> **Language & region**.
2. Add the "UTIME Sample IME" keyboard if not already present.
3. Open Notepad.
4. Switch to UTIME (Win+Space).
5. Type `nihao` and press Space. You should see `你好`.

## How to Uninstall

1. Open Admin Command Prompt.
2. Navigate to the DLL folder.
3. Run:
   ```cmd
   regsvr32 /u UTIME.dll
   ```
