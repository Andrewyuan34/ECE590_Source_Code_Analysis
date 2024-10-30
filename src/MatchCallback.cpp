#include "MatchCallback.h"


namespace myproject {

MyMatchCallback::MyMatchCallback(clang::DiagnosticsEngine &diagEngine)
    : diagEngine(diagEngine), count(0) {}

void MyMatchCallback::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    if(checks.find("dead-stores") != checks.end()) {
        llvm::outs() << "dead-stores\n";
    }else if(checks.find("unreachable-code") != checks.end()) {
        llvm::outs() << "unreachable-code\n";
    }else if(checks.find("uninitialized-variable") != checks.end()) {
        llvm::outs() << "uninitialized-variable\n";
    }else if(checks.find("loop-invariant") != checks.end()) {
        llvm::outs() << "loop-invariant\n";
    }else {
        llvm::outs() << "No checks specified. At least one check must be provided.\n";
    }
}

bool MyMatchCallback::AddCheck(const std::string& check) {
    auto [iter, inserted] = checks.insert(check);
    return inserted;
}

void MyMatchCallback::reportDeadStoreError(const clang::VarDecl *varDecl) {
    unsigned diagID = diagEngine.getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "Dead store detected: variable %0 is assigned but never used"
    );

    diagEngine.Report(varDecl->getLocation(), diagID) << varDecl->getName();
}

} // namespace myproject
