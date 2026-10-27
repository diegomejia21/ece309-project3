# Project 3 Design Log
**Author:** Diego Mejia · **P2 code used:** my own
**Tools used:** Claude Code (AI assistant) for help with implementation, testing and writing; Docker for Linux builds.

## 1. Hash table

**Collision strategy: separate chaining.** The table is a `std::vector` of buckets, each a `std::list<Entry>`, the "array of linked lists" design from lecture. An `Entry` holds the tool's name and a `std::unique_ptr<Tool>`. Storing the name avoids calling the virtual `name()` during lookups.

I chose chaining over linear probing because a collision never blocks an insert and it avoids the clustering of linear probing (about 30 probes with FNV-1a at 0.75, versus 6 for chaining).

**Hash function: 64-bit FNV-1a.** For each byte, XOR it into the hash, then multiply by the FNV prime. It uses every character, and the multiply makes it order-sensitive (`"ab"` ≠ `"ba"`). `hash()` returns the full 64-bit value. The index is `hash & (bucket_count - 1)`.

**Threshold and resize.** The table starts at 8 buckets and always has a power-of-two count. Before an `add`, if `size + 1 > bucket_count` (load factor above 1.0), the table doubles and every entry is re-inserted at its new index. Checking *before* inserting means the load factor is never above 1.0 when `add` returns. 1.0 is safe for chaining because the table never fills up: an average chain of at most one entry keeps `find` O(1).

**Measurements** (`tool_0` … `tool_9999`):

| Hash | bucket_count | load_factor | longest chain |
|---|---|---|---|
| FNV-1a (mine) | 16384 | 0.610 | **6** |
| `s[0] + s.size()` | 16384 | — | **9000** (4 buckets used) |

The weak hash sees only the first letter (always `t`) and the length (6–9), so all 10,000 names fall into 4 buckets, and `find` becomes a linear search.

**Why `add()` is amortized O(1).** An add without a resize is one hash, one short chain search, and one insert. A resize from n to 2n buckets moves n entries, but the previous resize left the table at n buckets holding only n/2 tools, so it takes n/2 cheap adds to trigger the next one. Spreading the O(n) cost over those adds gives O(1) extra each, the same doubling argument as `Conversation` in P2.

## 2. Pointer stability

The buckets hold `std::unique_ptr<Tool>`, not `Tool` objects. Each tool is allocated once by `std::make_unique`, and its address never changes. During `rehash`, each `Entry` is moved to its new bucket. Moving a `unique_ptr` copies only the pointer, so the `Tool` itself never moves, and a `Tool*` returned by `find()` stays valid. `reg_10000_tools` checks that the pointer to `tool_0` is unchanged after 11 resizes. `reg_tools_destroyed_exactly_once` shows that a rehash destroys nothing, a rejected duplicate is destroyed immediately, and the destructor frees each remaining tool once.

## 3. Calculator trace: `2 * (3 + 4) - 5`

- `2`: push → values `[2]`, ops `[]`.
- `*`: ops is empty → push → ops `[*]`.
- `(`: push → ops `[* (]`.
- `3`: values `[2 3]`.
- `+`: the top is `(`, not an operator, so nothing is applied. Push → ops `[* ( +]`.
- `4`: values `[2 3 4]`.
- `)`: apply until the top is `(`. Pop `+`, then 4 (right), then 3 (left), and push 7 → values `[2 7]`. Pop `(` → ops `[*]`.
- `-`: the top `*` has precedence 2 ≥ 1, so apply it: 2 × 7 = 14 → values `[14]`. Push `-` → ops `[-]`.
- `5`: values `[14 5]`.
- End: apply `-`: 14 − 5 = **9**, the single value left.

The first value popped is the *right* operand, and the `>=` rule makes `10 - 4 - 3` go left to right. My tests fail if either is changed.

## 4. Errors as values

In an agent loop a failing tool is normal: a missing file, a path outside the workspace, a division by zero. These are not harness bugs. They are information the model needs to try something else. So `resolve`, `calc_evaluate`, and every tool return failure as data (`false` plus a message, or `ToolResult::failure`), and the dispatcher hands it back as `"ok":false`.

Exceptions are for *programmer* errors, such as `as_string()` on a JSON number or `at()` out of range. Those mean the C++ code is wrong and should fail loudly. The dispatcher still catches anything a tool throws, so one buggy tool can't end the session.

## 5. What I would design differently

I would make each chain a `std::vector<Entry>` instead of a `std::list`. Chains average under one entry, so a vector avoids a heap node per tool and keeps entries together in memory. I used `std::list` to match the lecture's chaining design.
