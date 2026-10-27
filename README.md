# ECE 309 — Project 3: Tool Use and the Tool Registry

**Author:** Diego Mejia

The P2 `miniharness`, extended so the model can call tools (`read_file`,
`write_file`, `list_dir`, `calculator`). The harness runs each tool call,
adds the result to the conversation, and asks the model again until it
replies with plain text. See `P3_spec.md` for the full assignment.

## Files

My code:

- `include/tools/tool_registry.h` (private section), `src/tools/tool_registry.cpp`:
  hash table from tool name to `Tool`. Separate chaining (array of
  `std::list`), FNV-1a hash, power-of-two bucket count, doubles before the
  load factor would pass 1.0
- `src/tools/workspace.cpp`: `Workspace::resolve`, the lexical path check
  that keeps file tools inside the workspace
- `src/tools/calculator_eval.cpp`: `calc_evaluate`, two-stack evaluation
- `include/core/conversation.h`, `include/core/sentinel_scanner.h`,
  `src/core/conversation.cpp`, `src/core/sentinel_scanner.cpp`: my P2 code
- `tests/p3/test_p3.cpp`: 26 tests (`reg_`, `ws_`, `calc_`)
- `docs/design-log-p3.md`: design log

Everything else is the provided starter code, unmodified.

## Build and run (Linux / WSL2)

```bash
cmake -S . -B build              # ASan + UBSan on by default
cmake --build build -j
cd build && ctest --output-on-failure && cd ..
./build/test_p3                  # prints the 10,000-tool measurements
./build/miniharness --tools
./build/miniharness --script scripts/tools_demo.script
```

`tools_demo` writes `workspace/notes/summary.txt`; delete it to start over.
