#ifndef MATCH_CALLBACK_H
#define MATCH_CALLBACK_H

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/Diagnostic.h>
#include <llvm/Support/raw_ostream.h>
#include <format>
#include <string>

namespace myproject {

class MyMatchCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    explicit MyMatchCallback(clang::DiagnosticsEngine& diagEngine);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    void reportDeadStoreError(const clang::VarDecl *varDecl);
    clang::DiagnosticsEngine& diagEngine;
    unsigned count;
};

} // namespace myproject

#endif // MATCH_CALLBACK_H

