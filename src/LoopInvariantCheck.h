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

        // Define a lambda to process the loop body
        auto processBody = [this, &result](const clang::Stmt *Body) {
            if (Body) {
                analyzeStmt(Body, result);  // Main analysis function
            } else {
                llvm::outs() << "Loop body is null\n";  
            }
        };

        if (const clang::ForStmt* ForLoop = llvm::dyn_cast<clang::ForStmt>(S)) {
            llvm::outs() << "Found a for loop\n";
            processBody(ForLoop->getBody());  
        } else if (const clang::WhileStmt* WhileLoop = llvm::dyn_cast<clang::WhileStmt>(S)) {
            llvm::outs() << "Found a while loop\n";
            processBody(WhileLoop->getBody());  
        } else if (const clang::DoStmt* DoLoop = llvm::dyn_cast<clang::DoStmt>(S)) {
            llvm::outs() << "Found a do-while loop\n";
            processBody(DoLoop->getBody());  

        return true;
        } else {
            llvm::outs() << "No loop found\n";
            return false;
        }
    }
    return std::nullopt;
}



void LoopInvariantCheck::analyzeStmt(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) {
    for (const clang::Stmt *Child : S->children()) {
        if (!Child) continue;

        // Check loop invariant expressions
        if (isLoopInvariant(Child, S)) {
            reportLoopInvariant(Child, result);
        }
    }
}

bool LoopInvariantCheck::isLoopInvariant(const clang::Stmt *S, const clang::Stmt *LoopBody) {
    // Only handle binary operators for now
    if(const clang::BinaryOperator* B = llvm::dyn_cast<clang::BinaryOperator>(S)){
        if (!B->isAssignmentOp() || B->isCompoundAssignmentOp()) return false; 

        // Check if the expression is a constant
        const clang::Expr* RHS = B->getRHS();
        if (llvm::isa<clang::IntegerLiteral>(RHS) ||
            llvm::isa<clang::FloatingLiteral>(RHS) ||
            llvm::isa<clang::CharacterLiteral>(RHS)) {
            return true;
        }

        // Check if RHS is a DeclRefExpr, handling possible implicit casts
        if (const clang::ImplicitCastExpr *ICE = llvm::dyn_cast<clang::ImplicitCastExpr>(RHS)) {
            // If there is an implicit cast, get the subexpression
            RHS = ICE->getSubExpr();
        }

        if (const clang::DeclRefExpr* DRE = llvm::dyn_cast<clang::DeclRefExpr>(RHS)) {
            // Check if the variable is not modified in the loop and only handle situation for VarDecl
            const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl());
            if (VD && !isModifiedInLoop(VD, LoopBody)) { // No explicit check for VD
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
    for (const clang::Stmt* Child : LoopBody->children()) {
        if (const clang::BinaryOperator* BO = llvm::dyn_cast<clang::BinaryOperator>(Child)) {
            // Exclude situations for comparison operators
            if (BO->getOpcode() == clang::BO_EQ || 
                BO->getOpcode() == clang::BO_NE ||
                BO->getOpcode() == clang::BO_LT ||
                BO->getOpcode() == clang::BO_GT ||
                BO->getOpcode() == clang::BO_LE ||
                BO->getOpcode() == clang::BO_GE) {
                if (const clang::DeclRefExpr *LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) {
                    if (LHS->getDecl() == VD) {
                        return false;  
                    }
                }
            }

            if (BO->isAssignmentOp()) {
                if (const clang::DeclRefExpr *LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) { // Simplify the check by just checking the LHS
                    if (LHS->getDecl() == VD) {
                        return true; 
                    }
                }
            }
        }

        // Check for unary operators (self-increment or self-decrement)
        if (const clang::UnaryOperator* UO = llvm::dyn_cast<clang::UnaryOperator>(Child)) {
            if (UO->isIncrementDecrementOp()) {
                if (const clang::DeclRefExpr* LHS = llvm::dyn_cast<clang::DeclRefExpr>(UO->getSubExpr())) {
                    if (LHS->getDecl() == VD) {
                        return true;
                    }
                }
            }
        }

        // Recursively check children
        if (isModifiedInLoop(VD, Child)) {
            return true;
        }
    }
    return false; // Cannot find any modification
}

std::optional<bool> LoopInvariantCheck::reportLoopInvariant(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) const {
    if (!S) return std::nullopt; // Check if the statement is empty

    // Get the location of the statement
    clang::SourceLocation Loc = S->getBeginLoc();
    if (Loc.isInvalid()) return std::nullopt; // If the location is invalid, return

    // Setup the diagnostic engine
    clang::DiagnosticsEngine &Diag = result.Context->getDiagnostics();
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                           "Expression is loop-invariant and can be moved out of the loop");

    // Trigger engine to report the diagnostic
    Diag.Report(Loc, DiagID);
    return true;
}







/*
11.13 Update:
Just a basic framework for the LoopInvariantCheck. The next step is to implement the check method to find the loop invariant in the for loop.
Initial idea is to find the loop invariant by checking different loop(for loop, while loop, do while loop)

11.28 Update:
Refine this class by adding more detailed logic to find the loop invariant in the for loop, while loop and do while loop.
*/