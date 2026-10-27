// src/tools/workspace.cpp
// Author: Diego Mejia
//
// Workspace::resolve: a purely lexical check, steps 1–6 of spec §4.4.
// It never touches the filesystem, so it works for files that don't exist
// yet. The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    // 1. Empty path.
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    // 2. A NUL byte would silently cut the path short in the OS.
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }

    // 3. Only workspace-relative paths are allowed.
    fs::path p{std::string(user_path)};
    if (p.is_absolute() || p.has_root_path()) {
        *reason = "absolute paths are not allowed: " + std::string(user_path);
        return false;
    }

    // 4. Remove "." and duplicate slashes, fold "x/.." pairs.
    fs::path n = p.lexically_normal();

    // 5. A leading ".." means the path climbs above the workspace root.
    if (*n.begin() == "..") {
        *reason = "path escapes the workspace: " + std::string(user_path);
        return false;
    }

    // 6. Join with root.
    if (n == ".") {
        *out = root();
    } else {
        *out = root() / n;
    }
    return true;
}
