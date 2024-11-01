#include "MatchCallback.h"

namespace myproject {

MyMatchCallback::MyMatchCallback(clang::DiagnosticsEngine &diagEngine)
    : diagEngine(diagEngine), count(0), checks() {}

void MyMatchCallback::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    /*for (auto&& [name, strategy] : checks) {
        llvm::outs() << "Running check: " << name << "\n";
    }*/
    if(checks.find("dead-stores") != checks.end()) {
        checks["dead-stores"]->check(result);
    }else if(checks.find("unreachable-code") == checks.end()) {
        llvm::outs() << "unreachablecode is not found\n";
    }else if(checks.find("uninitialized-variable") == checks.end()) {
        llvm::outs() << "uninitializedvar is not found\n";
    }else if(checks.find("loop-invariant") == checks.end()) {
        llvm::outs() << "loopinvariant is not found\n";
    }else {
        llvm::outs() << "Unknown matcher type\n";
    }
}

// Add a check to the callback
bool MyMatchCallback::AddCheck(std::unique_ptr<CheckStrategy>&& check) {
    const std::string& checkName = check->getName();
    if (checks.find(checkName) != checks.end()) {
        llvm::errs() << "Check already exists: " << checkName << "\n";
        return false;
    }

    checks[checkName] = std::move(check);
    return true;
}

void MyMatchCallback::reportDeadStoreError(const clang::VarDecl *varDecl) {
    unsigned diagID = diagEngine.getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "Dead store detected: variable %0 is assigned but never used"
    );

    diagEngine.Report(varDecl->getLocation(), diagID) << varDecl->getName();
}

} // namespace myproject
