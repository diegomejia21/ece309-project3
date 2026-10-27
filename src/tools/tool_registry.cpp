// src/tools/tool_registry.cpp
// Author: Diego Mejia
//
// A hash table from tool name to Tool, using separate chaining (spec §4.3).
//   - hash:     64-bit FNV-1a over every character of the name
//   - index:    hash & (bucket_count - 1), bucket_count is a power of two
//   - resize:   double the bucket count BEFORE an add that would push the
//               load factor above 1.0, then re-insert every entry
// Tools are owned by std::unique_ptr inside the chains, so a rehash moves
// only the pointers; the Tool objects never move.

#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

// The unique_ptrs in the chains destroy every tool.
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;   // FNV offset basis
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;                  // FNV prime
    }
    return static_cast<std::size_t>(h);
}

std::size_t ToolRegistry::index_for(std::string_view key, std::size_t count) noexcept {
    return hash(key) & (count - 1);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;

    std::string name = tool->name();
    if (find(name) != nullptr) return false;    // `tool` is destroyed on return

    // Check the load factor before inserting, so it never exceeds 1.0.
    if (size_ + 1 > buckets_.size()) rehash(buckets_.size() * 2);

    std::list<Entry>& chain = buckets_[index_for(name, buckets_.size())];
    chain.push_back(Entry{std::move(name), std::move(tool)});
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const std::list<Entry>& chain = buckets_[index_for(name, buckets_.size())];
    for (const Entry& e : chain) {
        if (e.name == name) return e.tool.get();
    }
    return nullptr;
}

void ToolRegistry::rehash(std::size_t new_count) {
    std::vector<std::list<Entry>> bigger(new_count);
    for (std::list<Entry>& chain : buckets_) {
        for (Entry& e : chain) {
            // The index changes with the bucket count, so recompute it.
            std::size_t i = index_for(e.name, new_count);
            bigger[i].push_back(std::move(e));   // moves the unique_ptr, not the Tool
        }
    }
    buckets_ = std::move(bigger);
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const std::list<Entry>& chain : buckets_) {
        for (const Entry& e : chain) fn(*e.tool);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const std::list<Entry>& chain : buckets_) {
        if (chain.size() > longest) longest = chain.size();
    }
    return longest;
}
