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
    matchers.push_back(cadv::SingleMatcher(stmt(anyOf(forStmt(), whileStmt(), doStmt())).bind("loop_invariant")));
    
    return matchers;
}
std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) final;

private:
void analyzeStmt(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result);
bool isLoopInvariant(const clang::Stmt *E, const clang::Stmt *LoopBody);
bool isModifiedInLoop(const clang::VarDecl *VD, const clang::Stmt *LoopBody);
std::optional<bool> reportLoopInvariant(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) const;
};

std::optional<bool> LoopInvariantCheck::check(const clang::ast_matchers::MatchFinder::MatchResult &result) {
    if (const clang::Stmt *S = result.Nodes.getNodeAs<clang::Stmt>("loop_invariant")) {
        if (llvm::isa<clang::ForStmt>(S)) {
            llvm::outs() << "Found a for loop\n";
        } else if (llvm::isa<clang::WhileStmt>(S)) {
            llvm::outs() << "Found a while loop\n";
        } else if (llvm::isa<clang::DoStmt>(S)) {
            llvm::outs() << "Found a do-while loop\n";
        }

        if (const clang::Stmt *Body = S->getBody()) {
            analyzeStmt(Body, result); 
        } else {
            llvm::outs() << "Loop body is null\n";
        }

        return true;
    } else {
        llvm::outs() << "No loop found\n";
        return false;
    }
}

void LoopInvariantCheck::analyzeStmt(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) {
    for (const clang::Stmt *Child : S->children()) {
        if (!Child) continue;

        // Check loop invariant expressions
        if (isLoopInvariant(Child, S)) {
            llvm::outs() << "Found loop invariant expression: ";
            reportLoopInvariant(Child, result);
        }
    }
}

bool LoopInvariantCheck::isLoopInvariant(const clang::Stmt *S, const clang::Stmt *LoopBody) {
    // Only handle binary operators for now
    if(const clang::BinaryOperator* B = llvm::dyn_cast<clang::BinaryOperator>(S)){
        if (!B->isAssignmentOp()) return false; 

        const clang::Expr* RHS = B->getRHS();
        if (llvm::isa<clang::IntegerLiteral>(RHS) ||
            llvm::isa<clang::FloatingLiteral>(RHS) ||
            llvm::isa<clang::CharacterLiteral>(RHS)) {
            return true;
        }

        if (const clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(S)) {
            const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl());
            if (VD && !isModifiedInLoop(VD, LoopBody)) {
                return true; 
            }
            return false; 
        }

    }

    
    for (const clang::Stmt *Child : S->children()) {
        if (!isLoopInvariant(Child, LoopBody)) {
            return false;
        }
    }

    // Return false if no previous check passed
    return false;
}


bool LoopInvariantCheck::isModifiedInLoop(const clang::VarDecl *VD, const clang::Stmt *LoopBody) {
    // Traverse the loop body to find modifications to the variable
    for (const clang::Stmt *Child : LoopBody->children()) {
        if (const clang::BinaryOperator *BO = llvm::dyn_cast<clang::BinaryOperator>(Child)) {
            if (BO->isAssignmentOp()) {
                if (const clang::DeclRefExpr *LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) {
                    if (LHS->getDecl() == VD) {
                        return true; 
                    }
                }
            }
        }

        // If the child is a statement, recursively check for modifications
        if (const clang::Stmt *SubStmt = llvm::dyn_cast<clang::Stmt>(Child)) {
            if (isModifiedInLoop(VD, SubStmt)) {
                return true;
            }
        }
    }
    return false; // 未找到修改
}

std::optional<bool> LoopInvariantCheck::reportLoopInvariant(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) const {
    if (!S) return std::nullopt; // 如果语句为空，返回 std::nullopt

    // 获取源位置
    clang::SourceLocation Loc = S->getBeginLoc();
    if (Loc.isInvalid()) return std::nullopt; // 如果无法定位，返回 std::nullopt

    // 设置诊断引擎和自定义诊断消息
    clang::DiagnosticsEngine &Diag = result.Context->getDiagnostics();
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                           "Expression is loop-invariant and can be moved out of the loop");

    // 触发诊断
    Diag.Report(Loc, DiagID);
    return true;
}







/*
11.13 Update:
Just a basic framework for the LoopInvariantCheck. The next step is to implement the check method to find the loop invariant in the for loop.
Initial idea is to find the loop invariant by checking different loop(for loop, while loop, do while loop)
*/