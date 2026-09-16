# Verification

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

The build script reruns the simulation suite. The native integration check is available through `HungryHippos.exe --smoke-test`.

## Display correction

The original release passed simulation tests and offscreen image checks, but a subsequent live check and user report exposed flashing. Its paint handler cleared the visible client area before a relatively expensive scaled image copy. The original automated checks did not detect this problem.

The corrected handler scales and composes the entire image and margins into a second memory bitmap, flushes queued GDI drawing, and transfers the finished image to the window in one operation. There is no intermediate background clear on the visible surface. A short native window check was used because the reported flashing was uncomfortable; this is not a guarantee for every graphics driver or display. The game was closed after the check.

The same followup corrected a clipped pause title and preserved short key taps that arrive and end between timer updates. All four player keys now have a regression check for that case.
