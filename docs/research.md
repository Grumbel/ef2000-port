# Research conventions

Keep independently useful discoveries in focused Markdown files here, including
archive/data layouts, mission records, campaign state, save formats, and runtime
interfaces. Do not bury them exclusively in code or progress notes.

For each finding record:

- Executable/data-file SHA-256 and release identification, if known.
- Evidence: file offset, guest address with mapping/base, trace, or reproducer.
- Field width, byte order, signedness, units, and bounds for binary structures.
- Whether the result is observed, inferred, or still hypothetical.
- A small reproducible experiment and unanswered questions.

Distinguish file offsets, load-module offsets, segment:offset addresses, and guest
linear addresses. Never transfer addresses between releases without validation.
Label synthetic test fixtures as synthetic. Keep original installation data
unchanged; reference runs should use disposable copies because the game can write
configuration, campaign state, and saves.

No mission or campaign record layout has been established by this repository yet.
