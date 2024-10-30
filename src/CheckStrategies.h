#pragma once

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/Dynamic/VariantValue.h>
#include <vector>
#include <optional>

class CheckStrategy {
public:
    CheckStrategy(const std::string& name) : name_(name) {}
    virtual ~CheckStrategy() = default;

    // Simplify the type name
    using MatchersList = std::vector<clang::ast_matchers::dynamic::VariantMatcher>;
    
    // 返回多个匹配器的接口
    virtual MatchersList getMatchers() const = 0;
    virtual std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) = 0;

    const std::string& getName() const { return name_; }
private:
    std::string name_;
};

