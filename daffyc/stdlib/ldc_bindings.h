#pragma once

namespace daffyscript::stdlib {

struct LdcBinding {
    const char* module;
    const char* name;
    const char* signature;
};

inline constexpr LdcBinding ldc_bindings[] = {
    {"ldc", "message.send",    "(i32)->void"},
    {"ldc", "message.reply",   "(i32,i32)->void"},
    {"ldc", "event.emit",      "(i32,i32)->void"},
    {"ldc", "event.listen",    "(i32,i32)->void"},
    {"ldc", "storage.get",     "(i32)->i32"},
    {"ldc", "storage.set",     "(i32,i32)->void"},
    {"ldc", "storage.delete",  "(i32)->void"},
    {"ldc", "timer.set",       "(i32,i32)->void"},
    {"ldc", "timer.cancel",    "(i32)->void"},
    {"ldc", "room.peers",      "()->i32"},
    {"ldc", "room.name",       "()->i32"},
    {"ldc", "room.config",     "(i32)->i32"},
};

inline constexpr size_t ldc_binding_count = sizeof(ldc_bindings) / sizeof(ldc_bindings[0]);

} // namespace daffyscript::stdlib
