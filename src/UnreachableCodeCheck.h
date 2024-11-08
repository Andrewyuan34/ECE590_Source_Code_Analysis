#pragma once

#include "CheckStrategies.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <clang/Analysis/CFG.h>
#include "llvm/Support/raw_ostream.h"
#include <format>
#include <string>
#include <optional>
#include <assert.h>

class UnreachableCodeCheck : public CheckStrategy {
public:
typedef llvm::SmallSet<unsigned, 32> CFG_Set;
UnreachableCodeCheck(const std::string& name) : CheckStrategy(name) {}
MatchersList getMatchers() const final {
    using namespace clang::ast_matchers;
    using cadv = clang::ast_matchers::dynamic::VariantMatcher;
    
    MatchersList matchers;
    matchers.push_back(cadv::SingleMatcher(functionDecl(hasBody(stmt())).bind("unreachable_func")));
    
    return matchers;
}

    std::optional<bool> check(const clang::ast_matchers::MatchFinder::MatchResult& result) final;
private:
    CFG_Set reachable, visited;
    std::optional<bool> reportUnreachableCode(const clang::Stmt* stmt, const clang::SourceManager& sm);
    std::optional<bool> FindreachablePoints(const clang::CFGBlock* Block, CFG_Set& reachable, CFG_Set& visited);
    std::optional<bool> isInvalidPath(const clang::CFGBlock* Block);

};

// Recursively finds the entry point(s) for this dead CFGBlock.
std::optional<bool> UnreachableCodeCheck::FindreachablePoints(const clang::CFGBlock* Block, CFG_Set& reachable, CFG_Set& visited){
    visited.insert(Block->getBlockID());

    //if(Block->pred_size() == 0) return false; May have better way to check this
    for (const auto PreBlock : Block->preds()) {
        if (!PreBlock)
          continue;

        if (!reachable.count(PreBlock->getBlockID())) {
          // If we find an unreachable predecessor, mark this block as reachable so
          // we don't report this block
          reachable.insert(Block->getBlockID());
          if (!visited.count(PreBlock->getBlockID()))
            // If we haven't previously visited the unreachable predecessor, recurse
            FindreachablePoints(PreBlock, reachable, visited);
        }
    }
    return true;
}   

std::optional<bool> UnreachableCodeCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    const clang::SourceManager& sm = *result.SourceManager;
    if (const clang::FunctionDecl* FD = result.Nodes.getNodeAs<clang::FunctionDecl>("unreachable_func")) {
        if (!sm.isWrittenInMainFile(FD->getLocation())) {
            return {}; 
        }
        llvm::outs() << std::format("FUNCTION: {}\n", FD->getQualifiedNameAsString());
        // Generate the control flow graph (CFG)
        std::unique_ptr<clang::CFG> cfg = clang::CFG::buildCFG(FD, FD->getBody(), 
                                                               result.Context, clang::CFG::BuildOptions());
        assert(cfg != nullptr && "Failed to generate CFG for function");
        const clang::Stmt* lastStmt = nullptr;
        clang::SourceLocation lastLoc;
        clang::SourceRange lastRange;
        unsigned lastLine = 0;
        // Traverse all basic blocks in the CFG
        for (const clang::CFGBlock* Block : *cfg) {
                // Check if the block is unreachable
            if (reachable.count(Block->getBlockID())) continue;

            // Check if the block is empty (an artificial block)
            if (Block->empty()) continue;

            // Find the entry points for this block这里缺检查！！！！！！！！
            if (!visited.count(Block->getBlockID())) FindreachablePoints(Block, reachable, visited);

            // This block may have been pruned; check if we still want to report it
            if (reachable.count(Block->getBlockID())) continue;

            // Check for false positives
            if (isInvalidPath(Block)) continue;
        }







            /*
            bool isReachable = !Block->pred_empty(); // 判断该块是否有前驱节点
            // Only process unreachable blocks
            if (!isReachable) {
                for (const auto& Element : *Block) {
                    if (auto cfgStmt = Element.getAs<clang::CFGStmt>()) {
                        const clang::Stmt *stmt = cfgStmt->getStmt();
                        clang::SourceLocation loc = stmt->getBeginLoc();
                        // Get the line number of the current statement
                        unsigned currentLine = sm.getSpellingLineNumber(loc);
                        // If this is a statement on a different line, output the last statement on the previous line
                        if (lastStmt && currentLine != lastLine) {
                            // Call the helper function to report unreachable code
                            reportUnreachableCode(lastStmt, sm);
                        }
                        // 更新最后一个语句的位置信息
                        lastStmt = stmt;
                        lastLoc = loc;
                        lastRange = stmt->getSourceRange();
                        lastLine = currentLine;
                    }
                }
            }

        }
        // 如果最后一个语句不可达，输出它
        if (lastStmt) {
            reportUnreachableCode(lastStmt, sm);  // 调用 helper 函数报告不可达语句
        }
        }*/
    }
    return {};
}

// Helper function to report unreachable code
std::optional<bool> UnreachableCodeCheck::reportUnreachableCode(const clang::Stmt* stmt, const clang::SourceManager& sm) {
    if (!stmt) return std::nullopt;

    clang::SourceLocation loc = stmt->getBeginLoc();
    clang::DiagnosticsEngine &Diag = sm.getDiagnostics();
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                           "Unreachable code found at '%0'");
    Diag.Report(loc, DiagID) << sm.getFilename(loc).str() + ":" + std::to_string(sm.getSpellingLineNumber(loc));
    return true;
}


std::optional<bool> UnreachableCodeCheck::isInvalidPath(const clang::CFGBlock* Block) {

    if (Block->pred_size() > 1)
        return true;

    // If there are no predecessors, then this block is trivially unreachable
    if (Block->pred_size() == 0)
        return false;

    const auto pred = *Block->pred_begin();
    if (!pred) return false;

    // Get the predecessor block's terminator condition
    const clang::Stmt* cond = pred->getTerminatorCondition();


    if (!cond)
        return false;

    // Run each of the checks on the conditions这里没检查！！！！！！！！！！！！！！！！！！！！！！！
    return true;
}


