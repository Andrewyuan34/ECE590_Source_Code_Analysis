# pragma once

#include "CheckStrategies.h"
#include <clang/Analysis/CFG.h>
#include <clang/Analysis/AnalysisDeclContext.h>
#include <clang/Analysis/Analyses/LiveVariables.h>
#include "clang/AST/Attr.h"


class DeadStoreObserver : public clang::LiveVariables::Observer {
public:
    const clang::ast_matchers::MatchFinder::MatchResult &result;
    
    DeadStoreObserver(const clang::ast_matchers::MatchFinder::MatchResult &r) 
        : result(r) {}

    void observeStmt(const clang::Stmt *S, const clang::CFGBlock *currentBlock,
                     const clang::LiveVariables::LivenessValues &Live) override {

        // Skip statements in macros.
        if (S->getBeginLoc().isMacroID())
            return;

        // Only cover dead stores from regular assignments. ++/-- dead stores
        // have never flagged a real bug.
        if (const clang::BinaryOperator *B = llvm::dyn_cast<clang::BinaryOperator>(S)) {
            if (!B->isAssignmentOp())
                return; // Skip non-assignments.

            if (const clang::DeclRefExpr *DR = llvm::dyn_cast<clang::DeclRefExpr>(B->getLHS())) {
                if (const clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(DR->getDecl())) {

                    // Special case: self-assignments. These are often used to suppress
                    // "unused variable" compiler warnings.
                    const clang::Expr *RHS = B->getRHS()->IgnoreParenCasts();
                    if (const clang::DeclRefExpr *RhsDR = llvm::dyn_cast<clang::DeclRefExpr>(RHS)) {
                        if (VD == llvm::dyn_cast<clang::VarDecl>(RhsDR->getDecl()))
                            return; // Skip self-assignment
                    }

                    // Check if the variable declaration might be a dead store
                    CheckVarDecl(VD, DR, Live, result);
                }
            }
        }
    }

private:
    void reportDeadStore(const clang::VarDecl *VD, const clang::Expr *Ex) const {
        clang::SourceLocation Loc = Ex->getExprLoc();
        clang::DiagnosticsEngine &Diag = result.Context->getDiagnostics();
        unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                               "Value stored to '%0' is never read");
        Diag.Report(Loc, DiagID) << VD->getName();
    }

    void CheckVarDecl(const clang::VarDecl *VD, const clang::Expr *Ex,
                      const clang::LiveVariables::LivenessValues &Live,
                      const clang::ast_matchers::MatchFinder::MatchResult &result) const {
        
        if (!VD->hasLocalStorage())
            return;
        if (VD->getType()->getAs<clang::ReferenceType>())
            return;

        if (!Live.isLive(Live, VD) && !(VD->hasAttr<clang::UnusedAttr>())) {
            reportDeadStore(VD, Ex);
        }
    }
};


class DeadStoresCheck : public CheckStrategy {
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

    std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) override { //在这种情况下，似乎只能查到，初始化后直接又被赋值的变量
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
            auto observer = std::make_unique<DeadStoreObserver>();
	        assert(observer);
	        liveVars->runOnAllBlocks(*observer);
        }   
        return true;
    }
};

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
