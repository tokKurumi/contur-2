# LAN File Relay Demo

This demo models two processes (P1, P2) and two files (F1, F2) hosted on a LAN device.
P2 reads F2 over the LAN, computes a simple arithmetic expression, and passes the
result to P1 through shared memory. P1 waits for that signal and writes the result
back to the LAN file you choose.

## What This Demo Shows

- Two instruction programs running on the Contur CPU.
- Interrupt-driven I/O for a LAN device.
- Shared-memory handoff between processes.
- File operations performed through the Contur file system.
- Manual dumps of kernel state, registers, memory, and file contents.

## How To Run

Build the project, then run the demo executable:

- Build: use the standard Contur build scripts or CMake presets.
- Run: the executable target name is lan_file_relay_demo.

## Interactive Commands

- help: show command list.
- step: execute one kernel tick.
- run <N>: execute N kernel ticks.
- dump: print kernel snapshot, registers, shared memory, LAN status.
- files: print current contents of F1 and F2.
- quit: exit the demo.

## Expected Result

- P2 reads the initial value from F2, computes an expression, and writes the result
  into shared memory.
- P1 detects the shared-memory flag and writes the computed value to the chosen
  LAN file (F1 or F2).
- Dumps show the process states, registers, memory handoff, and file values.
