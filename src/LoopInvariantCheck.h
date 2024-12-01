#pragma once

#include "CheckStrategies.h"
#include <set>

bool isComparisonOperator(const clang::BinaryOperator* BO) {
    static const std::set<clang::BinaryOperatorKind> comparisonOps = {
        clang::BO_EQ, clang::BO_NE, clang::BO_LT, 
        clang::BO_GT, clang::BO_LE, clang::BO_GE
    };
    return comparisonOps.find(BO->getOpcode()) != comparisonOps.end();
}

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
bool isLoopInvariant(const clang::Stmt *E, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result);
bool isModifiedInLoop(const clang::VarDecl *VD, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result);
std::optional<bool> reportLoopInvariant(const clang::Stmt *S, const clang::ast_matchers::MatchFinder::MatchResult &result) const;
bool isRightOperandInvariant(const clang::Expr *RHS, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result);
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
        if (isLoopInvariant(Child, S, result)) {
            reportLoopInvariant(Child, result);
        }
    }
}

// Assume all unary operators will result in changes to the variable 
bool LoopInvariantCheck::isLoopInvariant(const clang::Stmt *S, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result) {
    // Only handle binary operators for now
    if(const clang::BinaryOperator* B = llvm::dyn_cast<clang::BinaryOperator>(S)){
        const clang::Expr* RHS = B->getRHS();
    

        if (B->getLHS()->isModifiableLvalue(*result.Context)) {
            return false;
        }

        if (B->getOpcode() == clang::BO_Assign){ // Handle situations for assignment operators
            // Check if the expression is a constant
            if (llvm::isa<clang::IntegerLiteral>(RHS) ||
                llvm::isa<clang::FloatingLiteral>(RHS) ||
                llvm::isa<clang::CharacterLiteral>(RHS)) {
                llvm::outs() << "Found a constant\n";
                return true;
            }
        }

        // Check if RHS is a CXXStaticCastExpr
        if (const clang::CXXStaticCastExpr *SCE = llvm::dyn_cast<clang::CXXStaticCastExpr>(RHS)) {
            // If it's a static cast, get the subexpression
            RHS = SCE->getSubExpr();
        }

        return isRightOperandInvariant(RHS, LoopBody, result);

    }

    for (const clang::Stmt *Child : S->children()) {
        if (!isLoopInvariant(Child, LoopBody, result)) {
            return false;
        }
    }

    // Return false if no previous check passed
    return false;
}



// Return true if the variable is modified in the loop(Only handle limited cases)
bool LoopInvariantCheck::isModifiedInLoop(const clang::VarDecl *VD, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result) {
    // Traverse the loop body to find modifications to the variable
    for (const clang::Stmt* Child : LoopBody->children()) {
        if (const clang::BinaryOperator* BO = llvm::dyn_cast<clang::BinaryOperator>(Child)) {
            // Exclude situations for comparison operators
            if (isComparisonOperator(BO)) {
                if (const clang::DeclRefExpr* LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) {
                    if (LHS->getDecl() == VD) {
                        return false;  
                    }
                }
            }

            if (BO->getOpcode() == clang::BO_Assign) {
                if (const clang::DeclRefExpr *LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) {
                    if (LHS->getDecl() == VD) {
                        // 调用 isRightOperandInvariant 函数来检查右操作数是否是 loop-invariant
                        if (BO->getRHS() && !isRightOperandInvariant(BO->getRHS(), LoopBody, result)) {
                            return true;
                        }
                    }
                }
            }
        }

        // Check for unary operators (self-increment or self-decrement)
        if (const clang::UnaryOperator* UO = llvm::dyn_cast<clang::UnaryOperator>(Child)) {
            if (UO->isIncrementDecrementOp() || UO->isArithmeticOp()) { // isIncrementDecrementOp() is used to check if the operator is ++ or --, isArithmeticOp() is used to check if the operator is + - ~ or !
                if (const clang::DeclRefExpr* Operand  = llvm::dyn_cast<clang::DeclRefExpr>(UO->getSubExpr())) {
                    if (Operand->getDecl() == VD) {
                        return true;
                    }
                }
            }
        }

        // Recursively check children
        if (isModifiedInLoop(VD, Child, result)) {
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

bool LoopInvariantCheck::isRightOperandInvariant(const clang::Expr *RHS, const clang::Stmt *LoopBody, const clang::ast_matchers::MatchFinder::MatchResult &result) {
    // 判断右操作数是否为 modifiable lvalue
    if (!RHS->isModifiableLvalue(*result.Context)) {
        return true;
    }

    // 检查右操作数是否是 DeclRefExpr
    if (const clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(RHS)) {
        // 判断是否是 VarDecl
        if (const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
            // 如果变量没有在循环中被修改，则认为右操作数是 loop-invariant
            return !isModifiedInLoop(VD, LoopBody, result);
        }
    }

    return false;
}








/*
11.13 Update:
Just a basic framework for the LoopInvariantCheck. The next step is to implement the check method to find the loop invariant in the for loop.
Initial idea is to find the loop invariant by checking different loop(for loop, while loop, do while loop)

11.28 Update:
Refine this class by adding more detailed logic to find the loop invariant in the for loop, while loop and do while loop.

11.29 Update:
Add more detailed logic to find the loop invariant, add a helper function to check if the variable is modified in the loop.

12.1 Update:
Further refine the logic to find the loop invariant in this class. Need to test the logic and make sure it works correctly.
*/