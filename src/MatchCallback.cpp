#include "MatchCallback.h"

namespace myproject {

MyMatchCallback::MyMatchCallback(clang::DiagnosticsEngine &diagEngine)
    : diagEngine(diagEngine), count(0), checks() {}


void MyMatchCallback::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    // why??? it is so werid that if i use if-else statement, the check in else if will not be executed
    if(checks.find("dead-stores") != checks.end()) {
        checks["dead-stores"]->check(result);
    }

    if(checks.find("unreachable-code") != checks.end()) {
        checks["unreachable-code"]->check(result);
    }
    
    if(checks.find("uninitialized-variable") != checks.end()) {
        llvm::outs() << "uninitializedvar is not found\n";
    }
    
    if(checks.find("loop-invariant") != checks.end()) {
        checks["loop-invariant"]->check(result);
    }

}



void MyMatchCallback::onEndOfTranslationUnit() {
    for(auto&& [name, strategy] : checks) {
    }
}

// Add a check to the callback
bool MyMatchCallback::AddCheck(std::unique_ptr<CheckStrategy>&& check) {
    if (!check) {  // Make sure the check is not null
        llvm::errs() << "Error: Attempted to add a null check.\n";
        return false;
    }

    const std::string& checkName = check->getName();
    if (checks.find(checkName) != checks.end()) {
        llvm::errs() << "Check already exists: " << checkName << "\n";
        return false;
    }

    checks[checkName] = std::move(check);
    return true;
}


} // namespace myproject
