# pragma once

#include "CheckStrategies.h"

class DeadStoresCheck : public CheckStrategy {
public:
    MatchersList getMatchers() const override {
        using namespace clang::ast_matchers;
        using cadv = clang::ast_matchers::dynamic::VariantMatcher;
        
        // Create a list of matchers
        MatchersList matchers;

        // 1. Find all function declarations
        matchers.push_back(cadv::SingleMatcher(
                functionDecl(isExpansionInMainFile()).bind("funcDecl"))
        );

        // 2. Find all loop statements
        matchers.push_back(cadv::SingleMatcher(stmt(anyOf(
                        forStmt().bind("forLoop"),
                        whileStmt().bind("whileLoop"),
                        doStmt().bind("doLoop")
                    ), isExpansionInMainFile()
                )
            )
        );
        
        return matchers;
    }

    std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
        llvm::outs() << "DeadStoresCheck!!!!!!!!!!!!!!!!!\n";
        return true;
    }
};
