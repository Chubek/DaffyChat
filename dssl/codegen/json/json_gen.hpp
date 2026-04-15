#pragma once
#include "../../ast/ast.h"
#include "../../ast/ast_printer.hpp"
#include <string>
#include <fstream>

namespace dssl::codegen {

class JsonGenerator {
public:
    bool generate(const ast::File& file, const std::string& out_dir) {
        std::string path = out_dir + "/" + basename(file.filename) + ".meta.json";
        std::ofstream ofs(path);
        if (!ofs) return false;
        ofs << ast::PrintFile(file);
        ofs.close();
        return true;
    }

private:
    static std::string basename(const std::string& path) {
        auto pos = path.rfind('/');
        auto name = (pos == std::string::npos) ? path : path.substr(pos + 1);
        auto dot = name.rfind('.');
        return (dot == std::string::npos) ? name : name.substr(0, dot);
    }
};

} // namespace dssl::codegen
