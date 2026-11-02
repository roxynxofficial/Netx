# NETX Cybersecurity Framework

NETX 1.0.0 is a C++20 interactive command terminal for conservative network diagnostics, passive web connectivity checks, local file hashing, and basic forensic review. It uses the C++ standard library and operating-system networking APIs; Windows hashing/random generation uses CNG.

## Build

Requirements: CMake 3.20+, C++20 compiler (MSVC 2022 or GCC/Clang), Windows 10/11 or Linux.

Windows (from a Visual Studio Developer PowerShell):

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/netx.exe` on Windows (or `build/Release/netx.exe` with multi-configuration generators) and `build/netx` on Linux. No dependency installation is required.

## Use

Run `netx` for the interactive prompt or pass a command after the executable, for example `netx version` or `netx dnsx lookup example.org`. Enter `help` for commands. Reports are written as JSON and TXT under `./reports` relative to the current working directory.

Useful commands: `help`, `sysinfo`, `modules`, `theme Matrix`, `netmap ports HOST --range 1-128`, `nping HOST`, `dnsx lookup HOST`, `wmap audit https://HOST`, `tracex hash FILE`, `cryptx random`.

## Scope and safety

Only test hosts you own or are explicitly authorized to assess. Port checks are limited to 2,048 ports per invocation, use a one-second timeout, and add a conservative pause. WMAP currently checks DNS and TCP reachability; it does not fetch HTTP content, inspect TLS certificates, or parse response headers. ICMP ping, full interface enumeration, MX/NS/TXT DNS queries, full local hardening checks, and Linux hashing are not implemented and are reported as such. NETX does not run shell commands or exploit targets.

The initial version provides lightweight command history, basic settings, aliases, colored output, and TXT/JSON reports. Configuration persistence, command completion, CSV, service fingerprinting, and complete HTTP/TLS auditing remain future work.

## Development

Build with `-DCMAKE_BUILD_TYPE=Debug` on single-configuration generators. The parser test is registered with CTest. Use `ctest --test-dir build --output-on-failure` to run it.
