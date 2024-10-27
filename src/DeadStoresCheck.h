# pragma once

#include "CheckStrategies.h"

class DeadStoresCheck : public CheckStrategy {
public:
    MatchersList getMatchers() const override {
        using namespace clang::ast_matchers;
        using csdv = clang::ast_matchers::dynamic::VariantMatcher;
        
        // Create a list of matchers
        MatchersList matchers;
        matchers.push_back(csdv::SingleMatcher(varDecl(hasInitializer(expr().bind("deadStore")), isExpansionInMainFile()).bind("varDecl")));
        matchers.push_back(csdv::SingleMatcher(varDecl(hasType(isInteger()), isExpansionInMainFile()).bind("deadStoreInt")));
        
        return matchers;
    }
};
