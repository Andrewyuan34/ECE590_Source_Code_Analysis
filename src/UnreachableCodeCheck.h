#pragma once

#include "CheckStrategies.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

class UnreachableCodeCheck : public CheckStrategy {
public:
    UnreachableCodeCheck(const std::string& name) : CheckStrategy(name) {}

    MatchersList getMatchers() const final {
        using namespace clang::ast_matchers;
        using cadv = clang::ast_matchers::dynamic::VariantMatcher;


        MatchersList matchers;

        matchers.push_back(cadv::SingleMatcher(
            stmt(
                hasAnySubstatement(returnStmt()), 
                hasDescendant(stmt(unless(returnStmt())))
            ).bind("unreachableCode")  
        ));
        
        return matchers;
    }

    std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;
};

std::optional<bool> UnreachableCodeCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    if (const clang::Stmt* S = result.Nodes.getNodeAs<clang::Stmt>("unreachableCode")) {
        clang::SourceLocation Loc = S->getBeginLoc();
        clang::DiagnosticsEngine &Diag = result.Context->getDiagnostics();
        unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                               "Unreachable code detected");
        Diag.Report(Loc, DiagID);
        return true;
    }
    return false;
}
