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
#include <queue>

class UnreachableCodeCheck : public CheckStrategy {
public:

typedef llvm::SmallSet<unsigned, 32> CFG_Set;

UnreachableCodeCheck(const std::string& name) : CheckStrategy(name) {
    unreachableBlocks.reserve(8);    
}

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
    std::vector<const clang::CFGBlock*> unreachableBlocks;
    std::optional<bool> reportUnreachableCode(const clang::Stmt* stmt, const clang::SourceManager& sm);
    const clang::Stmt* getUnreachableStmt(const clang::CFGBlock *Block);
    std::optional<bool> markReachableBlocks(const clang::CFG *cfg, CFG_Set &reachable);
};

std::optional<bool> UnreachableCodeCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    const clang::SourceManager& sm = *result.SourceManager;
    if (const clang::FunctionDecl* FD = result.Nodes.getNodeAs<clang::FunctionDecl>("unreachable_func")) {
        if (!sm.isWrittenInMainFile(FD->getLocation())) {
            return {}; 
        }
        // Generate the control flow graph (CFG)
        std::unique_ptr<clang::CFG> cfg = clang::CFG::buildCFG(FD, FD->getBody(), 
                                                               result.Context, clang::CFG::BuildOptions());
        assert(cfg != nullptr && "Failed to generate CFG for function");

        // Mark reachable blocks
        auto reachableResult = markReachableBlocks(cfg.get(), reachable);
        if (!reachableResult.has_value()) {
            llvm::outs() << "No reachable blocks found\n";
            return std::nullopt; // No reachable blocks found
        }

        // Identify unreachable blocks
        for (const auto *Block : *cfg) {
            if (!Block) continue;
            if (!reachable.count(Block->getBlockID())) {
                unreachableBlocks.push_back(Block);
            }
        }

        // Report each unreachable block in reverse order
        for(auto Block : llvm::reverse(unreachableBlocks)) {
            const clang::Stmt *S = getUnreachableStmt(Block);
            if (S) {
                reportUnreachableCode(S, sm);
            } else {
                llvm::outs() << "Unreachable block without statement\n";
            }
        }
        visited.clear();
        reachable.clear();
        unreachableBlocks.clear();
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

// Find the Stmt* in a CFGBlock for reporting a warning
const clang::Stmt* UnreachableCodeCheck::getUnreachableStmt(const clang::CFGBlock *Block) {
  for (const clang::CFGElement& Elem : *Block) {
    if (std::optional<clang::CFGStmt> S = Elem.getAs<clang::CFGStmt>()) {
      if (!llvm::isa<clang::DeclStmt>(S->getStmt()))
        return S->getStmt();
    }
  }
  return Block->getTerminatorStmt();
}

// Performs BFS from the entry block to mark reachable nodes
std::optional<bool> UnreachableCodeCheck::markReachableBlocks(const clang::CFG *cfg, CFG_Set &reachable) {
    if (!cfg) return std::nullopt;  // Return std::nullopt if cfg is null

    std::queue<const clang::CFGBlock*> queue;
    queue.push(&cfg->getEntry());
    reachable.insert(cfg->getEntry().getBlockID());

    while (!queue.empty()) {
        const clang::CFGBlock* Block = queue.front();
        queue.pop();

        for (const auto &Succ : Block->succs()) {
            if (Succ && !reachable.count(Succ->getBlockID())) {
                reachable.insert(Succ->getBlockID());
                queue.push(Succ);
            }
        }
    }

    if (reachable.empty()) {
        return std::nullopt; // Return std::nullopt if no reachable blocks were marked
    }

    return true; // Successfully marked reachable blocks
}


