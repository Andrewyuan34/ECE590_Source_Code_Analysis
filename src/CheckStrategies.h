#pragma once

#include <clang/ASTMatchers/ASTMatchers.h>
#include <vector>

class CheckStrategy {
public:
    virtual ~CheckStrategy() = default;

    // Simplify the type name
    using MatchersList = std::vector<clang::ast_matchers::dynamic::VariantMatcher>;
    
    // 返回多个匹配器的接口
    virtual MatchersList getMatchers() const = 0;
};

