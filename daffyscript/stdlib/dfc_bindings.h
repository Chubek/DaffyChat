#pragma once

namespace daffyscript::stdlib {

struct DfcBinding {
    const char* module;
    const char* name;
    const char* signature;
};

inline constexpr DfcBinding dfc_bindings[] = {
    {"dfc", "bridge.emit",     "(i32,i32)->void"},
    {"dfc", "bridge.on_hook",  "(i32,i32)->void"},
    {"dfc", "bridge.off_hook", "(i32)->void"},
    {"dfc", "types.Color.new", "(i32,i32,i32)->i32"},
    {"dfc", "types.Color.to_hex", "(i32)->i32"},
};

inline constexpr size_t dfc_binding_count = sizeof(dfc_bindings) / sizeof(dfc_bindings[0]);

} // namespace daffyscript::stdlib
