#pragma once
#include "../ast/ast.h"
#include <string>

namespace dssl::codegen {

class Target {
public:
    virtual ~Target() = default;
    virtual std::string name() const = 0;
    virtual bool generate(const ast::File& file, const std::string& out_dir) = 0;
};

} // namespace dssl::codegen
