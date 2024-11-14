#pragma once

#include "CheckStrategies.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

class LoopInvariantCheck : public CheckStrategy {
public:

LoopInvariantCheck(const std::string& name) : CheckStrategy(name) {}
MatchersList getMatchers() const final {
    using namespace clang::ast_matchers;
    using cadv = clang::ast_matchers::dynamic::VariantMatcher;
    
    MatchersList matchers;
    matchers.push_back(cadv::SingleMatcher(forStmt().bind("loop_invariant")));
    
    return matchers;
}
std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) final;
};

std::optional<bool> LoopInvariantCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& result){
    if (const clang::ForStmt* FS = result.Nodes.getNodeAs<clang::ForStmt>("loop_invariant")) {
        llvm::outs() << "Found a for loop\n";
    }
    return std::nullopt;
}


/*
11.13 Update:
Just a basic framework for the LoopInvariantCheck. The next step is to implement the check method to find the loop invariant in the for loop.
Initial idea is to find the loop invariant by checking different loop(for loop, while loop, do while loop)
*/