#pragma once
#include "../ast/ast.h"
#include "../diag/diagnostic.hpp"
#include <string>
#include <fstream>
#include <sstream>

namespace daffyscript::codegen {

class WasmEmitter {
public:
    explicit WasmEmitter(DiagEngine& diag) : diag_(diag) {}

    bool emit(const ast::File& file, const std::string& out_path) {
        std::string wat = generateWat(file);

        std::string wat_path = out_path;
        auto dot = wat_path.rfind('.');
        if (dot != std::string::npos) {
            wat_path = wat_path.substr(0, dot) + ".wat";
        } else {
            wat_path += ".wat";
        }

        std::ofstream ofs(wat_path);
        if (!ofs) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E200",
                "Cannot write to " + wat_path,
                {file.filename, 0, 0}, {}});
            return false;
        }
        ofs << wat;
        ofs.close();
        return true;
    }

private:
    DiagEngine& diag_;

    std::string generateWat(const ast::File& file) {
        std::ostringstream os;
        os << "(module\n";

        os << "  ;; DaffyChat " << ast::FileTypeStr(file.file_type)
           << ": " << file.name << " v" << file.version << "\n\n";

        os << "  (memory (export \"memory\") 1)\n\n";

        emitImports(os, file);
        emitExports(os, file);

        os << ")\n";
        return os.str();
    }

    void emitImports(std::ostringstream& os, const ast::File& file) {
        os << "  ;; Imports from DaffyChat runtime\n";

        if (file.file_type == ast::FileType::Module) {
            os << "  (import \"dfc\" \"bridge.emit\" (func $dfc_bridge_emit (param i32 i32)))\n";
            os << "  (import \"dfc\" \"bridge.on_hook\" (func $dfc_bridge_on_hook (param i32 i32)))\n";
        } else if (file.file_type == ast::FileType::Program) {
            os << "  (import \"ldc\" \"message.send\" (func $ldc_message_send (param i32)))\n";
            os << "  (import \"ldc\" \"event.emit\" (func $ldc_event_emit (param i32 i32)))\n";
            os << "  (import \"ldc\" \"storage.get\" (func $ldc_storage_get (param i32) (result i32)))\n";
            os << "  (import \"ldc\" \"storage.set\" (func $ldc_storage_set (param i32 i32)))\n";
        }
        os << "\n";
    }

    void emitExports(std::ostringstream& os, const ast::File& file) {
        os << "  ;; Exported entry points\n";

        if (file.file_type == ast::FileType::Module) {
            os << "  (func $__module_init (export \"__module_init\")\n";
            os << "    nop\n";
            os << "  )\n";
        } else if (file.file_type == ast::FileType::Program) {
            os << "  (func $__program_init (export \"__program_init\")\n";
            os << "    nop\n";
            os << "  )\n";
        } else if (file.file_type == ast::FileType::Recipe) {
            os << "  (func $__recipe_init (export \"__recipe_init\")\n";
            os << "    nop\n";
            os << "  )\n";
        }
        os << "\n";
    }
};

} // namespace daffyscript::codegen
