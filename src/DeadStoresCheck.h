# pragma once

#include "CheckStrategies.h"
#include <clang/Analysis/CFG.h>
#include <clang/Analysis/AnalysisDeclContext.h>
#include <clang/Analysis/Analyses/LiveVariables.h>
#include "clang/AST/Attr.h"


class DeadStoresCheck : public CheckStrategy {
friend class DeadStoreObserver;
public:
DeadStoresCheck(const std::string& name) : CheckStrategy(name) {}
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

std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;
};

// Inheriting from clang::LiveVariables::Observer, the program can perform custom analysis on the liveness of variables by running runOnAllBlocks(*observer)
class DeadStoreObserver : public clang::LiveVariables::Observer {
public:
const clang::ast_matchers::MatchFinder::MatchResult& result;

struct DeadStoreInfo {
    const clang::VarDecl *VD;
    const clang::Expr *Ex;
};

DeadStoreObserver(const clang::ast_matchers::MatchFinder::MatchResult& r) : result(r) {}

void observeStmt(const clang::Stmt* S, const clang::CFGBlock* currentBlock, const clang::LiveVariables::LivenessValues& Live) final;

// Reverse iterate through the stack to report dead stores in order
void reportAllDeadStores() const {
    for (auto it = ReportStack.rbegin(); it != ReportStack.rend(); ++it) {
        reportDeadStore(it->VD, it->Ex);
    }
}

private:
mutable std::vector<DeadStoreInfo> ReportStack;  // Stack to store dead stores

std::optional<bool> reportDeadStore(const clang::VarDecl *VD, const clang::Expr *Ex) const;
std::optional<bool> CheckVarDecl(const clang::VarDecl *VD, const clang::Expr *Ex,
                                 const clang::LiveVariables::LivenessValues &Live,
                                 const clang::ast_matchers::MatchFinder::MatchResult &result) const;

};


// So many buggy issues not checking in the function
void DeadStoreObserver::observeStmt(const clang::Stmt* S, const clang::CFGBlock* currentBlock, const clang::LiveVariables::LivenessValues& Live){
    // Skip statements in macros.
    if (S->getBeginLoc().isMacroID())
        return;
    // Only cover dead stores from regular assignments. Skip ++/-- operators.
    if (const clang::BinaryOperator* B = llvm::dyn_cast<clang::BinaryOperator>(S)) {
        // Skip non-assignments.
        if (!B->isAssignmentOp())
            return; 
        if (const clang::DeclRefExpr* DR = llvm::dyn_cast<clang::DeclRefExpr>(B->getLHS())) {
            if (const clang::VarDecl* VD = llvm::dyn_cast<clang::VarDecl>(DR->getDecl())) {
                // Special case: self-assignments. These are often used to suppress
                // "unused variable" compiler warnings.
                const clang::Expr *RHS = B->getRHS()->IgnoreParenCasts();
                if (const clang::DeclRefExpr *RhsDR = llvm::dyn_cast<clang::DeclRefExpr>(RHS)) {
                    if (VD == llvm::dyn_cast<clang::VarDecl>(RhsDR->getDecl()))
                        return; // Skip self-assignment
                }
                // Check if the variable declaration might be a dead store
                if(!CheckVarDecl(VD, DR, Live, result)) {
                    llvm::outs() << "CheckVarDecl failed\n";
                    std::abort();
                }
                else return;
            }
        }
    }
    else if (const clang::DeclStmt *DS = dyn_cast<clang::DeclStmt>(S)) {
        // Iterate through the decls. Warn if any initializers are complex
        // expressions that are not live (never used).
        for (const auto *DI : DS->decls()) {
            const auto *V = dyn_cast<clang::VarDecl>(DI);

            if (!V)
                continue;

            if (V->hasLocalStorage()) {
                // Reference types confuse the dead stores checker.
                if (V->getType()->getAs<clang::ReferenceType>())
                    continue;

                // Get the initializer expression
                const clang::Expr *E = V->getInit();

                // If there is no initializer, continue
                if (!E) 
                    continue;

                // Don't warn on C++ objects (yet) until we can show that their
                // constructors/destructors don't have side effects.
                if (isa<clang::CXXConstructExpr>(E))
                    continue;

                // Check if the variable declaration might be a dead store
                if (!Live.isLive(V) && !V->hasAttr<clang::UnusedAttr>()) {
                    // Pass the variable declaration and the initializer expression to the report stack
                    ReportStack.push_back({V, E});
                    return;
                }
            }
        }
    }
}

// ReportDeadStore with optional return type and error handling
std::optional<bool> DeadStoreObserver::reportDeadStore(const clang::VarDecl *VD, const clang::Expr *Ex) const {
    if (!VD || !Ex) return std::nullopt;  // Error if pointers are null
    
    clang::SourceLocation Loc = Ex->getExprLoc();
    clang::DiagnosticsEngine &Diag = result.Context->getDiagnostics();
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                           "Value stored to '%0' is never read");
    Diag.Report(Loc, DiagID) << VD->getName();
    return true;
}

// CheckVarDecl implementation with error handling
std::optional<bool> DeadStoreObserver::CheckVarDecl(const clang::VarDecl *VD, const clang::Expr *Ex,
                                                    const clang::LiveVariables::LivenessValues &Live,
                                                    const clang::ast_matchers::MatchFinder::MatchResult &result) const {
    if (!VD || !Ex) return std::nullopt;  // Error if pointers are null

    if (!VD->hasLocalStorage()) return true;  // Skip non-local storage variables

    if (VD->getType()->getAs<clang::ReferenceType>()) return true;  // Skip reference types

    if (!Live.isLive(VD) && !VD->hasAttr<clang::UnusedAttr>()) ReportStack.push_back({VD, Ex});  // Report dead store if conditions met

    return true;
}

std::optional<bool> DeadStoresCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& result){ 
    
    if(auto funcDecl = result.Nodes.getNodeAs<clang::FunctionDecl>("funcDecl")) {
        clang::ASTContext *astContext = result.Context;
        clang::Stmt *funcBody = funcDecl->getBody();
        if (!funcBody) return false;
    
        llvm::outs() << std::format("FUNCTION: {}\n", funcDecl->getQualifiedNameAsString());
        // 获取当前函数的 CFG
        clang::AnalysisDeclContextManager manager(*astContext);
        clang::AnalysisDeclContext *AC = manager.getContext(funcDecl);
        AC->getCFGBuildOptions().setAllAlwaysAdd();//why？？？？加了这行以后就可以生成正常CFG了
        //AC->getCFGBuildOptions().AddLifetime = true;
        const clang::CFG *cfg = AC->getCFG();
        if (!cfg) {
            llvm::errs() << "Could not generate CFG for function.\n";
            return false;
        }
        // 构建 LiveVariables 分析器
        clang::LiveVariables* liveVars = AC->getAnalysis<clang::LiveVariables>(); 
        if (!liveVars) return false;
        auto observer = std::make_unique<DeadStoreObserver>(result);
        assert(observer);
        liveVars->runOnAllBlocks(*observer);
        observer->reportAllDeadStores();
    }   
    return true;
}

/*
So the essence of checking dead store is: In a funcion, you can only detect them when they have condition control flow otherwise the CFG will only generate 
one block for the whole function.

while loop will be sperate into three blocks in CFG, the first block is the condition block, the second block is the body block, the third block is the block that is
used to jump back to the condition block. and if there is nested while loop exists, the CFG would do the same thing(dividing the while loop into three blocks).

the for loop will be sperate into three blocks in CFG as well, the first block is the condition block, the second block is the body block, 
the third block is the block that is only used to jump back to the condition block. Interestingly, the initialization of index will be placed one block before the condition block.

the do while loop will be sperate into three blocks in CFG as well, the first block is the jump block(which doesnt mean anything), the second block is the loop body,
the third block is the condition block.

if else stmt will be sperate into two blocks in CFG, the first block is the if block body, the second block is the else block body.
That's werid. It seems that they deal with else if stmt by subtituting the else block for its place.
But whatever it is. Using if stmt definitely lead to extra block in CFG, which is what I wanna to see in this case.


*/
