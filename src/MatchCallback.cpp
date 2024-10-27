#include "MatchCallback.h"


namespace myproject {

MyMatchCallback::MyMatchCallback(clang::DiagnosticsEngine &diagEngine)
    : diagEngine(diagEngine), count(0) {}

void MyMatchCallback::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    llvm::outs() << std::format("MATCH {}:\n", count);

    if (auto varDecl = result.Nodes.getNodeAs<clang::VarDecl>("varDecl")) {
        std::string s(varDecl->getQualifiedNameAsString());
        llvm::outs() << std::format("dump for VarDecl {}:\n", s);
        varDecl->dump();
        ++count;
        llvm::outs() << std::format("Number of matches: {}\n", this->count);

        // 调用封装的错误报告函数
        reportDeadStoreError(varDecl);
    }
    else if (auto unusedFunc = result.Nodes.getNodeAs<clang::FunctionDecl>("unusedFunc")) {
        std::string s(unusedFunc->getQualifiedNameAsString());
        llvm::outs() << std::format("dump for FunctionDecl {}:\n", s);
        unusedFunc->dump();
        ++count;
        llvm::outs() << std::format("Number of matches: {}\n", this->count);
    }
    else {
        llvm::outs() << "No match found\n";
    }
}

void MyMatchCallback::reportDeadStoreError(const clang::VarDecl *varDecl) {
    unsigned diagID = diagEngine.getCustomDiagID(
        clang::DiagnosticsEngine::Error,
        "Dead store detected: variable %0 is assigned but never used"
    );

    diagEngine.Report(varDecl->getLocation(), diagID) << varDecl->getName();
}

} // namespace myproject
