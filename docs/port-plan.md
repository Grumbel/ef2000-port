# Port architecture

## Fidelity contract

Execute or translate original game logic, including the renderer, flight model,
AI, avionics, mission generation, and campaign. Do not replace these with
approximations. Preserve integer overflow, fixed-point arithmetic, x87 behavior,
memory aliasing, calling conventions, and observable timing where required.
Exact equivalence is a validation target, not an assumption about a new runtime.

The sibling `tfx3view` project can inform data-format research. It is not a
replacement for the original rendering or simulation code.

## Intended layers

1. A version-specific loader maps the original image, applies verified
   relocations, constructs startup state, and records address mappings.
2. An execution layer initially runs original x86 instructions. Choose the
   emulator/translator after recovering the protected-mode layout and observing
   hardware dependencies; do not build a new general-purpose x86 emulator.
3. An interface layer handles DOS/DPMI services and hardware accesses explicitly.
   Initially emulate devices when that preserves behavior more reliably than a
   high-level replacement. Log unimplemented interfaces and fail visibly.
4. Linux presentation and input adapters expose the original framebuffer,
   palette, audio output, and input semantics.
5. Translated and eventually decompiled functions replace instruction execution
   individually, retaining guest addresses and a bridge to untranslated code.

Full-system DOS emulation remains a reference and debugging tool. It is not by
itself completion of the native port.

## Milestones and acceptance evidence

1. **Recover loading:** fingerprint the installed release, decode the extender
   header and relocation stream, locate the protected-mode entry, and export a
   reproducible mapped image. Static stage loading, segment relocations, payload
   boundaries, and the 32-bit entry are recovered; runtime patches remain.
2. **Capture a reference:** boot a disposable copy under an instrumented emulator;
   record startup registers, memory mappings, DOS calls, interrupts, port I/O,
   file access, and a main-menu checkpoint. Identify unpacking/self-modification.
3. **Reach the menu in the compatibility runtime:** execute original code with
   explicit interface coverage; compare framebuffer/palette and file accesses.
4. **Run a complete mission:** retain original rendering and simulation. Compare
   controlled inputs, simulation state, audio events, and save/reload behavior.
   Control clock, random seed, and asynchronous events before claiming determinism.
5. **Translate and decompile progressively:** validate functions against original
   execution using identical guest state, including flags and memory side effects.
   Expand to campaigns and other releases only with versioned evidence.

## Next investigation

The [native harness](startup-runtime.md) executes original startup through stack
reservation, heap growth, and the beginning of runtime initialization. The core
non-DPMI `3509h`/`3508h` calculations now match instrumented reference records.
Native x86-64 compatibility mode is preferred because the QEMU comparison
backend did not enforce the tested expand-down bound.

Next: recover DOS time-query behavior and introduce a deliberate clock model,
then continue tracing initializer interfaces. Record time inputs for repeatable
validation; do not introduce a constant time response that can silently hang
later polling loops. Validate x87 setup, exception behavior, and hardware access
as those paths are reached. The current profile still has synthetic allocation
addresses and is not a complete DOS/protected-mode environment.
