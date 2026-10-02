# Verification

## October 2, 2026 recheck

- Rebuilt the current source with LLVM MinGW 20260922 (Clang 23.1.2), with warnings treated as errors. The full build passed under both PowerShell 7.6.5 and Windows PowerShell 5.1.
- All 240 simulated rounds completed: 234 cleared every marble and six reached the time limit. Ownership, score conservation, physics, and round-state checks passed.
- Both the shipped executable and the rebuilt executable passed the native window check, including presentation pixels at four window sizes, short taps for all four players, and stable GDI resource counts.
- A visible run of the rebuilt game exercised the lobby, countdown, Space chomp, keyboard pause, mouse resume, a complete round ending in a tie, four-player selection, and replay resetting all scores and marbles. This was a brief check on one Windows machine.
- The build now runs the native window check automatically. A local failure check confirmed that a missing report rejects the build even when a stale `PASS` report existed beforehand; a working directory containing spaces was also exercised.

The shipped executable is unchanged. Its SHA-256 is `f30313727b0a1f47eef14620ea46b8505621ba43fe73684043a3e6739e5e78cc`.

Speaker output still has not been verified by listening, and four people sharing a physical keyboard have not been tested.

## September 15, 2026 checks

Built and checked locally on September 15, 2026.

| Check | Result |
| :--- | :--- |
| C11 release build with Clang 23.1.1 | Passed with all enabled warnings treated as errors |
| 240 simulated full rounds | All finished; 234 consumed every marble, 6 ended at the time limit |
| Score and marble accounting | No duplicated captures or missing marbles in any checked simulation step |
| Collision physics | Finite positions and velocities, arena bounds, and wall reflection passed |
| Round state | Countdown, timeout, pause, resume, restart, ties, and frozen results passed |
| Native window integration | Window creation, timer, paint handler, keyboard and mouse actions passed; short taps for all four player keys are retained until the next simulation step |
| Focus behavior | Losing focus clears held keys and pauses the round |
| Graphics resource check | GDI object count unchanged after 60 full renders |
| Presentation regression check | Actual presentation function copies verified logo, sidebar, and margin pixels at 1200 by 820, 960 by 700, 1500 by 900, and 850 by 610; eight frames per size |
| Visual check | After the display fix, a brief live native window check showed the board in the lobby, countdown, active CPU round, and paused state; the earlier blank captures did not recur |
| Runtime dependencies | Imports only Windows system libraries and the Windows Universal C Runtime |

The sound generator and sound toggle are implemented and compiled. Speaker output has not been verified by listening. Multiplayer controls were exercised programmatically; four people sharing a physical keyboard was not tested.

The build script now reruns both the simulation suite and the native integration check. The native check is also available separately through `HungryHippos.exe --smoke-test`.

## Display correction

The original release passed simulation tests and offscreen image checks, but a subsequent live check and user report exposed flashing. Its paint handler cleared the visible client area before a relatively expensive scaled image copy. The original automated checks did not detect this problem.

The corrected handler scales and composes the entire image and margins into a second memory bitmap, flushes queued GDI drawing, and transfers the finished image to the window in one operation. There is no intermediate background clear on the visible surface. A short native window check was used because the reported flashing was uncomfortable; this is not a guarantee for every graphics driver or display. The game was closed after the check.

The same followup corrected a clipped pause title and preserved short key taps that arrive and end between timer updates. All four player keys now have a regression check for that case.
