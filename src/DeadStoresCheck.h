# pragma once

#include "CheckStrategies.h"
#include <clang/Analysis/CFG.h>
#include <clang/Analysis/AnalysisDeclContext.h>
#include <clang/Analysis/Analyses/LiveVariables.h>

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

    std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
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
            clang::LiveVariables *liveVars = AC->getAnalysis<clang::LiveVariables>(); 
	        if (!liveVars) return false;
            auto observer = std::make_unique<clang::LiveVariables::Observer>();
	        assert(observer);
	        liveVars->runOnAllBlocks(*observer);
            /*auto livenessvalues = std::make_unique<clang::LiveVariables::LivenessValues>();
            assert(livenessvalues);*/
	        //liveVars->dumpBlockLiveness((funcDecl->getASTContext()).getSourceManager());

            // 遍历 CFG 中的每个块，检查是否有死存储
            for (const clang::CFGBlock *block : *cfg) {
                for (auto &elem : *block) {
                    // 获取 CFGStmt
                    if (std::optional<clang::CFGStmt> cfgStmtOpt = elem.getAs<clang::CFGStmt>()) {
                        const clang::CFGStmt &cfgStmt = *cfgStmtOpt;
                        const clang::Stmt *stmt = cfgStmt.getStmt();
                            // 检查赋值操作
                            if (const clang::BinaryOperator *binOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
                                // 只关注赋值操作
                                if (binOp->isAssignmentOp()) {
                                    // 获取 LHS（左值表达式）
                                    if (const clang::DeclRefExpr *lhs = llvm::dyn_cast<clang::DeclRefExpr>(binOp->getLHS())) {
                                        // 获取 VarDecl
                                        if (const clang::VarDecl *varDecl = llvm::dyn_cast<clang::VarDecl>(lhs->getDecl())) {
                                            // 检查变量在当前块的末尾是否活跃
                                            if (!liveVars->isLive(block, varDecl)) {
                                                llvm::outs() << "Dead store detected in function '"
                                                             << funcDecl->getQualifiedNameAsString() << "' at "
                                                             << varDecl->getLocation().printToString(*result.SourceManager)//这里的输出是定义的位置，而不是使用的位置
                                                             << "\n";
                                                llvm::outs() << "Variable name: " << varDecl->getNameAsString() << "\n";
                                                if (varDecl->getType().getAsString() != "") {
                                                    llvm::outs() << "Variable type: " << varDecl->getType().getAsString() << "\n";
                                                }
                                                llvm::outs() << "Assigned at: ";
                                                binOp->getSourceRange().print(llvm::outs(), *result.SourceManager);
                                                llvm::outs() << "\n";
                                                llvm::outs() << "Statement: '";
                                                binOp->printPretty(llvm::outs(), nullptr, clang::PrintingPolicy(astContext->getLangOpts()));
                                                llvm::outs() << "\n";
                                                //varDecl->getSourceRange().print(llvm::outs(), *result.SourceManager);
                                                //lhs->printPretty(llvm::outs(), nullptr, clang::PrintingPolicy(astContext->getLangOpts()));
                                            }
                                        }
                                    }
                            }
                        }
                    }
                }
            }
        }   
        return true;
    }
};
