# TaskMesh Developer Guide

This document contains step-by-step instructions for building, running, testing, and developing TaskMesh on Windows using Microsoft Visual C++ (MSVC) and CMake.

---

## 1. Prerequisites & Environment

The project is built and verified on Windows with:
- **Operating System**: Windows 11 / Windows Server (x64)
- **C++ Standard**: C++20 (`/std:c++20`)
- **Compiler**: Microsoft Visual C++ (MSVC) 19.51+ (Visual Studio 2026 / Visual Studio Build Tools 2026)
- **Windows SDK**: Windows SDK 10.0.26100.0 or compatible
- **Build System**: CMake 3.24+ (tested on CMake 4.3.1)
- **Dependency Management**: CMake `FetchContent` (Git required for automatic GoogleTest download)

---

## 2. Setting Up the Environment

If CMake or MSVC are not in your default system `PATH`, you can initialize them in PowerShell:

```powershell
# Add MSVC CMake and Ninja to current session PATH
$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
```

Alternatively, open a **Developer PowerShell for VS 2026** or **x64 Native Tools Command Prompt**.

---

## 3. Configuring the Build

### Standard CMake Configuration (Visual Studio Generator)
From the repository root directory:

```powershell
# Configure solution for x64 architecture
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
```

### Using CMakePresets
TaskMesh provides predefined presets in `CMakePresets.json`:

```powershell
# Configure using presets
cmake --preset windows-msvc-debug
# or
cmake --preset windows-msvc-release
```

---

## 4. Building the Project

### Debug Configuration
```powershell
cmake --build build --config Debug
```

### Release Configuration
```powershell
cmake --build build --config Release
```

### Artifact Locations:
- Core Library: `build/Debug/taskmesh_core.lib` (or `build/Release/taskmesh_core.lib`)
- CLI Executable: `build/Debug/taskmesh_cli.exe` (or `build/Release/taskmesh_cli.exe`)
- Test Executable: `build/tests/Debug/taskmesh_tests.exe` (or `build/tests/Release/taskmesh_tests.exe`)

---

## 5. Running Tests

### Option A: Via CTest (Recommended)
```powershell
# Run Debug tests with failure output
ctest --test-dir build -C Debug --output-on-failure

# Run Release tests
ctest --test-dir build -C Release --output-on-failure
```

### Option B: Direct GoogleTest Binary
```powershell
# Run Debug test binary
.\build\tests\Debug\taskmesh_tests.exe

# Run Release test binary
.\build\tests\Release\taskmesh_tests.exe

# Filter specific tests
.\build\tests\Debug\taskmesh_tests.exe --gtest_filter=PrioritySchedulerTest.*
```

---

## 6. Running the CLI Demonstration

The CLI demonstrates domain validation, interleaved priority enqueues, deterministic equal-priority FIFO tie-breaking, and execution dispatch:

```powershell
# Run Debug CLI
.\build\Debug\taskmesh_cli.exe

# Run Release CLI
.\build\Release\taskmesh_cli.exe
```

---

## 7. Compiler Flags & Warning Discipline

TaskMesh enforces clean, warning-free compilation under `/W4` with standard conformance:
- `/W4`: High-level compiler warnings.
- `/permissive-`: Strict C++ standard conformance mode in MSVC.
- `/EHsc`: Standard C++ stack unwinding exception model.
- `/Zc:__cplusplus`: Correct `__cplusplus` macro reporting.

All pull requests and commits must compile with zero warnings on MSVC.

---

## 8. Adding New Tests

1. Place new test files or cases in `tests/`.
2. Use GoogleTest macros:
   - `EXPECT_EQ`, `EXPECT_NE`, `EXPECT_LT`, `EXPECT_GE` for value invariants.
   - `EXPECT_THROW` with explicit types (`JobValidationError`) for error cases.
3. Link the target to `taskmesh_core` and `GTest::gtest_main` in `tests/CMakeLists.txt`.
4. Ensure all tests are deterministic (use fixed seeds if using pseudo-random generation).
