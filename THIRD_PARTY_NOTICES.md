# Third-party notices

The game's C source, drawn artwork and synthesized sound effects are covered by [LICENSE](LICENSE). Hungry Hippos is an independent fan game; the software license does not grant rights to third-party trademarks.

The Windows build uses LLVM MinGW 20260922, including Clang 23.1.2 and MinGW-w64 headers and runtime. The compiler itself is not included in the playable package. Applicable toolchain notices are retained in `third-party/`:

- `LICENSE-llvm-mingw.txt`: LLVM's Apache 2.0 license with LLVM exceptions and included legacy notices.
- `COPYING.MinGW-w64.txt` and `COPYING.MinGW-w64-runtime.txt`: MinGW-w64 copyright and runtime notices from the same toolchain archive.

The executable imports Windows system libraries and the Windows Universal C Runtime. It does not include a game framework, external artwork, fonts or audio files. Text uses the Windows-installed Segoe UI font.
