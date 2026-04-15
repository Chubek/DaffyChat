#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct DSSLTarget DSSLTarget;
typedef struct ASTNode ASTNode;
typedef struct ASTList ASTList;

typedef void (*DSSLTargetInitFn)(DSSLTarget* self, const char* out_dir);
typedef void (*DSSLTargetFinalizeFn)(DSSLTarget* self);
typedef void (*DSSLTargetEmitStructFn)(DSSLTarget* self, ASTNode* node);
typedef void (*DSSLTargetEmitEnumFn)(DSSLTarget* self, ASTNode* node);
typedef void (*DSSLTargetEmitUnionFn)(DSSLTarget* self, ASTNode* node);
typedef void (*DSSLTargetEmitRpcFn)(DSSLTarget* self, ASTNode* node);
typedef void (*DSSLTargetEmitRestFn)(DSSLTarget* self, ASTNode* node);
typedef void (*DSSLTargetEmitExecFn)(DSSLTarget* self, ASTNode* node);

struct DSSLTarget {
    const char* name;
    void* priv;
    DSSLTargetInitFn init;
    DSSLTargetFinalizeFn finalize;
    DSSLTargetEmitStructFn emit_struct;
    DSSLTargetEmitEnumFn emit_enum;
    DSSLTargetEmitUnionFn emit_union;
    DSSLTargetEmitRpcFn emit_rpc;
    DSSLTargetEmitRestFn emit_rest;
    DSSLTargetEmitExecFn emit_exec;
};

typedef DSSLTarget* (*DSSLPluginCreateFn)(void);
typedef void (*DSSLPluginDestroyFn)(DSSLTarget*);

#ifdef __cplusplus
}
#endif
