// tests/p3/test_p3.cpp — P3 test suite.
// Author: Diego Mejia
//
// Areas: reg_ for the registry, ws_ for the workspace, calc_ for the
// calculator. The first five tests are the starter's worked examples.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Counts how many times a tool is destroyed, to check ownership.
class CountedTool : public NamedTool {
public:
    CountedTool(std::string n, int* deaths) : NamedTool(std::move(n)), deaths_(deaths) {}
    ~CountedTool() override { ++*deaths_; }
private:
    int* deaths_;
};

std::string tool_name(int i) { return "tool_" + std::to_string(i); }

// Tokenize with the provided tokenizer, then run calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

bool evals_to(const std::string& expr, double expected) {
    double v = 0;
    std::string err;
    return eval(expr, &v, &err) && v == expected;
}

// resolve() accepts `in` and maps it to `expected`.
bool accepts(const Workspace& ws, const std::string& in, const fs::path& expected) {
    fs::path out;
    std::string reason;
    return ws.resolve(in, &out, &reason) && out == expected;
}

// resolve() rejects `in` and explains why.
bool rejects(const Workspace& ws, const std::string& in) {
    fs::path out;
    std::string reason;
    return !ws.resolve(in, &out, &reason) && !reason.empty();
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ---------------------------------------------------------------- registry

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("calculator") == nullptr);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
}

TEST(reg_find_absent) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("read_file")));
    CHECK(r.find("read_fil") == nullptr);     // prefix
    CHECK(r.find("read_file2") == nullptr);   // longer
    CHECK(r.find("") == nullptr);
    CHECK(r.find("read_file") != nullptr);
    CHECK(r.size() == 1);
}

TEST(reg_tools_destroyed_exactly_once) {
    int deaths = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountedTool>("a", &deaths)));
        REQUIRE(r.add(std::make_unique<CountedTool>("b", &deaths)));
        CHECK(!r.add(std::make_unique<CountedTool>("a", &deaths)));   // the new one dies now
        CHECK(deaths == 1);
        for (int i = 0; i < 100; ++i)                                  // several resizes
            REQUIRE(r.add(std::make_unique<CountedTool>(tool_name(i), &deaths)));
        CHECK(deaths == 1);                                            // rehash destroyed nothing
    }
    CHECK(deaths == 1 + 102);                                          // registry freed the rest
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    const std::vector<std::string> names = {"read_file", "write_file", "list_dir",
                                            "calculator", "echo", "search"};
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));

    std::vector<std::string> seen;
    int count = 0;
    r.for_each([&](const Tool& tool) {
        seen.push_back(tool.name());
        ++count;
    });
    CHECK(count == static_cast<int>(names.size()));

    std::vector<std::string> expected = names;
    std::sort(expected.begin(), expected.end());
    std::sort(seen.begin(), seen.end());
    CHECK(seen == expected);   // each name exactly once
}

TEST(reg_hash_is_a_real_string_hash) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));   // order matters
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("calculator") == ToolRegistry::hash("calculator"));

    std::vector<std::size_t> hashes;
    for (int i = 0; i < 10000; ++i) hashes.push_back(ToolRegistry::hash(tool_name(i)));
    std::sort(hashes.begin(), hashes.end());
    auto distinct = std::unique(hashes.begin(), hashes.end()) - hashes.begin();
    CHECK(distinct >= 9990);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>(tool_name(0));
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));

    bool load_ok = r.load_factor() <= 1.0;
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>(tool_name(i))));
        if (r.load_factor() > 1.0) load_ok = false;
    }
    CHECK(load_ok);
    CHECK(r.size() == 10000);

    bool all_found = true;
    for (int i = 0; i < 10000; ++i) {
        Tool* t = r.find(tool_name(i));
        if (t == nullptr || t->name() != tool_name(i)) all_found = false;
    }
    CHECK(all_found);
    CHECK(r.find(tool_name(0)) == first_ptr);   // survived every resize
    CHECK(r.max_probe_length() <= 64);

    std::printf("    FNV-1a + chaining: bucket_count=%zu load_factor=%.3f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>(tool_name(i))));
    const std::size_t buckets = r.bucket_count();

    // Where the weak hash s[0] + s.size() would put the same names.
    std::vector<int> counts(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = tool_name(i);
        std::size_t weak = static_cast<unsigned char>(s[0]) + s.size();
        ++counts[weak % buckets];
    }
    int weak_longest = *std::max_element(counts.begin(), counts.end());
    long weak_used = std::count_if(counts.begin(), counts.end(), [](int c) { return c > 0; });

    std::printf("    buckets=%zu  FNV-1a longest chain=%zu  "
                "s[0]+s.size() longest chain=%d (%ld buckets used)\n",
                buckets, r.max_probe_length(), weak_longest, weak_used);
    CHECK(static_cast<std::size_t>(weak_longest) > 100 * r.max_probe_length());
}

// ---------------------------------------------------------------- workspace

TEST(ws_accepts_plain_and_normalized) {
    TempDir d;
    Workspace ws(d.path());
    const fs::path root = ws.root();
    CHECK(accepts(ws, "notes/todo.txt", root / "notes" / "todo.txt"));
    CHECK(accepts(ws, "./notes//todo.txt", root / "notes" / "todo.txt"));
    CHECK(accepts(ws, "a/../b", root / "b"));
    CHECK(accepts(ws, "my..notes.txt", root / "my..notes.txt"));
}

TEST(ws_accepts_root_itself) {
    TempDir d;
    Workspace ws(d.path());
    CHECK(accepts(ws, ".", ws.root()));
    CHECK(accepts(ws, "a/..", ws.root()));
}

TEST(ws_accepts_missing_file_without_creating_it) {
    TempDir d;
    Workspace ws(d.path());
    CHECK(accepts(ws, "new/dir/file.txt", ws.root() / "new" / "dir" / "file.txt"));
    CHECK(!fs::exists(ws.root() / "new"));   // resolve never touches the filesystem
}

TEST(ws_rejects_empty_and_nul) {
    TempDir d;
    Workspace ws(d.path());
    CHECK(rejects(ws, ""));
    CHECK(rejects(ws, std::string("notes") + '\0' + "/x"));
}

TEST(ws_rejects_absolute) {
    TempDir d;
    Workspace ws(d.path());
    CHECK(rejects(ws, "/etc/passwd"));
    CHECK(rejects(ws, ws.root().string()));   // absolute even though it points inside root
}

TEST(ws_rejects_dotdot) {
    TempDir d;
    Workspace ws(d.path());
    CHECK(rejects(ws, ".."));
    CHECK(rejects(ws, "../etc/passwd"));
    CHECK(rejects(ws, "a/../../b"));
}

TEST(ws_rejects_escape_that_comes_back) {
    TempDir d;
    fs::create_directories(d.path() / "ws");
    Workspace ws(d.path() / "ws");
    CHECK(rejects(ws, "notes/../../ws/notes/todo.txt"));
}

// ---------------------------------------------------------------- calculator

TEST(calc_spec_table) {
    CHECK(evals_to("2 + 3 * 4", 14));
    CHECK(evals_to("(2 + 3) * 4", 20));
    CHECK(evals_to("2 * (3 + 4) - 5", 9));
    CHECK(evals_to("-3 * -3", 9));
    CHECK(evals_to("7 / 2", 3.5));
}

TEST(calc_left_to_right) {
    CHECK(evals_to("10 - 4 - 3", 3));
    CHECK(evals_to("100 / 10 / 5", 2));
    CHECK(evals_to("8 - 2 * 3 + 1", 3));
}

TEST(calc_division_by_zero_in_parens) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_my_expressions) {
    CHECK(evals_to("3.25 * 4", 13));
    CHECK(evals_to("((1 + 2)) * (3 + 4) / 7", 3));
    CHECK(evals_to("(-1) - -1", 0));
    CHECK(evals_to("0.5 + 0.25", 0.75));
}

TEST(calc_bad_tokens_do_not_crash) {
    using K = CalcToken::Kind;
    double v = 0;
    std::string err;
    std::vector<CalcToken> lonely_op = {{K::Plus, 0}};
    CHECK(!calc_evaluate(lonely_op, &v, &err));
    CHECK(!err.empty());
    std::vector<CalcToken> close_only = {{K::Number, 1}, {K::RParen, 0}};
    CHECK(!calc_evaluate(close_only, &v, &err));
    std::vector<CalcToken> empty;
    CHECK(!calc_evaluate(empty, &v, &err));
}

TEST(calc_very_long_expressions) {
    std::string sum = "1";
    for (int i = 1; i < 20000; ++i) sum += "+1";
    CHECK(evals_to(sum, 20000));

    std::string nested = std::string(5000, '(') + "7" + std::string(5000, ')');
    CHECK(evals_to(nested, 7));
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
