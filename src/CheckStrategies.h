#pragma once

#include <clang/ASTMatchers/ASTMatchers.h>
#include <vector>
#include <optional>

class CheckStrategy {
public:
    virtual ~CheckStrategy() = default;

    // Simplify the type name
    using MatchersList = std::vector<clang::ast_matchers::dynamic::VariantMatcher>;
    
    // 返回多个匹配器的接口
    virtual MatchersList getMatchers() const = 0;
    virtual std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) = 0;
};

