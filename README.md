# Power Patch

A compact Windows app that starts Windows, Microsoft Store, and Microsoft 365 update checks from one panel. Download and installation progress stays in each updater.

## Setup

**Packaged app:** On Windows 11 x64, extract the complete release package and run `bin\powerpatch.exe`, or run the self-extracting `powerpatch.exe`. Store and Office updates require their respective apps to be installed and enabled.

**Build from source:**

1. Install Visual Studio with the Desktop C++ workload and Windows SDK, CMake 3.22+, and Qt 6 Widgets/Concurrent using the `msvc2022_64` kit.
2. Open an **x64 Native Tools Command Prompt** in the repository folder.
3. Run the commands below, adjusting both Qt paths to your installed version:

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="C:\Qt\6.10.1\msvc2022_64\lib\cmake\Qt6"
cmake --build build
set "PATH=C:\Qt\6.10.1\msvc2022_64\bin;%PATH%"
build\powerpatch.exe
```

Select your update sources, then click **Run selected updates**.

## Documentation

- [User guide and troubleshooting](docs/user-guide.md)
- [Development setup and scripts](docs/dev-environment.md)
- [Tests and regression checks](docs/update-flow-checks.md)
- [Packaging and versioning](docs/standalone-versioning.md)
- [Architecture](docs/architecture.md)

[MIT license](LICENSE).
