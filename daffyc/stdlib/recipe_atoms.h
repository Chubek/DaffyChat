#pragma once

namespace daffyscript::stdlib {

enum class RecipeAtom {
    Enabled,
    Disabled,
    Strict,
    Moderate,
    Relaxed,
};

inline const char* RecipeAtomName(RecipeAtom a) {
    switch (a) {
        case RecipeAtom::Enabled: return "enabled";
        case RecipeAtom::Disabled: return "disabled";
        case RecipeAtom::Strict: return "strict";
        case RecipeAtom::Moderate: return "moderate";
        case RecipeAtom::Relaxed: return "relaxed";
    }
    return "unknown";
}

} // namespace daffyscript::stdlib
