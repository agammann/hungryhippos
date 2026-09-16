# Verification

Built and checked locally on September 15, 2026.

| Check | Result |
| :--- | :--- |
| C11 release build with Clang 23.1.1 | Passed with all enabled warnings treated as errors |
| 240 simulated full rounds | All finished; 234 consumed every marble, 6 ended at the time limit |
| Score and marble accounting | No duplicated captures or missing marbles in any checked simulation step |
| Collision physics | Finite positions and velocities, arena bounds, and wall reflection passed |
| Round state | Countdown, timeout, pause, resume, restart, ties, and frozen results passed |
| Native window integration | Window creation, timer, paint handler, keyboard and mouse actions passed |
| Focus behavior | Losing focus clears held keys and pauses the round |
| Graphics resource check | GDI object count unchanged after 60 full renders |
| Visual check | Inspected images produced by the actual C renderer |
| Runtime dependencies | Imports only Windows system libraries and the Windows Universal C Runtime |

The sound generator and sound toggle are implemented and compiled. Speaker output has not been verified by listening. Multiplayer controls were exercised programmatically; four people sharing a physical keyboard was not tested.

The build script reruns the simulation suite. The native integration check is available through `HungryHippos.exe --smoke-test`.
