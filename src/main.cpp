#include <format>
#include <string>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/Dynamic/VariantValue.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include "Diagnostic_Engine.h"

namespace ct = clang::tooling;
namespace cam = clang::ast_matchers;
namespace lc = llvm::cl;

// 定义命令行选项，接受 0 到多个值
static lc::list<std::string> Checks(
    "checks", lc::desc("Specify checks to run (dead-stores, dead-code, uninit-vars, loop-inv)"), 
    lc::ZeroOrMore, // 指定接受0到多个值
    lc::value_desc("check"));

cam::dynamic::VariantMatcher getMatcher(const std::string &type) {
    using namespace cam;
    if (type == "dead-stores") {
        return dynamic::VariantMatcher::SingleMatcher(varDecl(hasInitializer(expr().bind("deadStore")), isExpansionInMainFile()).bind("varDecl"));
    } /*else if (type == "dead-code") {
        return;
    } else if (type == "uninit-vars") {
        return;
    } else if (type == "loop-invariant") {
        return;
    }*/
    llvm::errs() << "Unknown matcher type: " << type << "\n";
    return dynamic::VariantMatcher();
}

cam::dynamic::VariantMatcher traverse(clang::TraversalKind kind,
  cam::dynamic::VariantMatcher matcher) {
    using namespace cam;
    if (matcher.hasTypedMatcher<clang::Decl>()){return dynamic::VariantMatcher::SingleMatcher(traverse(kind, matcher.getTypedMatcher<clang::Decl>()));}
    else std::abort();
    /*} else if(matcher.hasTypedMatcher<xxxxxxxxx>()) {
        return cam::dynamic::VariantMatcher::SingleMatcher(traverse(kind, matcher.getTypedMatcher<xxxxxxxxxxxx>()));
    } else if(matcher.hasTypedMatcher<xxxxxxxxxxxx>()) {
        return cam::dynamic::VariantMatcher::SingleMatcher(traverse(kind, matcher.getTypedMatcher<xxxxxxxxxxx>()));
    } */
    
  }


struct MyMatchCallback : public cam::MatchFinder::MatchCallback {
    MyMatchCallback() : count(0) {}
    void run(const cam::MatchFinder::MatchResult& result) override;
    unsigned count;
};

void MyMatchCallback::run(const cam::MatchFinder::MatchResult& result) {
    llvm::outs() << std::format("MATCH {}:\n", count);
    
    if (auto varDecl = result.Nodes.getNodeAs<clang::VarDecl>("varDecl")) {
        std::string s(varDecl->getQualifiedNameAsString());
        llvm::outs() << std::format("dump for VarDecl {}:\n", s);
        varDecl->dump();

        // 获取 DiagnosticsEngine
        if (result.Context) {
            clang::DiagnosticsEngine &diagEngine = result.Context->getDiagnostics();

            // 创建诊断ID，用于报告警告或错误
            unsigned diagID = diagEngine.getCustomDiagID(clang::DiagnosticsEngine::Error, 
                                                         "Dead store detected: variable %0 is assigned but never used");

            // 报告错误，%0 表示第一个参数，会替换为变量名
            diagEngine.Report(varDecl->getLocation(), diagID) << varDecl->getName();
        }
    }
}

// 自定义的 ASTConsumer
class MyASTConsumer : public clang::ASTConsumer {
public:
    explicit MyASTConsumer(MatchFinder &Finder) : Finder(Finder) {}

    // 这个方法在每个翻译单元结束时被调用
    void HandleTranslationUnit(clang::ASTContext &Context) override {
        Finder.matchAST(Context);
    }

private:
    MatchFinder &Finder;
};

// 定义自定义的 ASTFrontendAction
class MyFrontendAction : public clang::ASTFrontendAction {
public:
    MyFrontendAction(CustomDiagnosticEngine &diagEngine) : diagEngine(diagEngine) {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file) override {
        matchCallback = std::make_unique<MyMatchCallback>(diagEngine);
        auto matchFinder = std::make_unique<cam::MatchFinder>();

        for (const auto &check : Checks) {
            matchFinder->addDynamicMatcher(*traverse(
                clAsIs ? clang::TK_AsIs : clang::TK_IgnoreUnlessSpelledInSource, getMatcher(check)).getSingleMatcher(),
                matchCallback.get());
        }

        return std::make_unique<MyASTConsumer>(matchFinder);
    }

    void EndSourceFileAction() override {
        llvm::outs() << std::format("Number of matches: {}\n", matchCallback->getCount());
    }

private:
    std::unique_ptr<MyMatchCallback> matchCallback;
    CustomDiagnosticEngine &diagEngine; // 使用自定义诊断引擎
};

static lc::OptionCategory optionCategory("Tool options");
static lc::opt<bool> clAsIs("i", lc::desc("Implicit nodes"),
  lc::cat(optionCategory));

int main(int argc, const char **argv) {
	auto optParser = ct::CommonOptionsParser::create(argc, argv, optionCategory);

	if (!optParser) {
		llvm::errs() << llvm::toString(optParser.takeError());
		return 1;
	}

	ct::ClangTool tool(optParser->getCompilations(), optParser->getSourcePathList());

	MyMatchCallback matchCallback;
	cam::MatchFinder matchFinder;
    
    CustomDiagnosticEngine diagEngine;
    // 设置诊断消费者
    
    assert(Checks.size() > 0);

    for(auto&& check : Checks)  {
		matchFinder.addDynamicMatcher(*traverse(
		                clAsIs ? clang::TK_AsIs : clang::TK_IgnoreUnlessSpelledInSource,
		                getMatcher(check)).getSingleMatcher(), &matchCallback);
	}

    int status = tool.run(ct::newFrontendActionFactory<MyFrontendAction>(diagEngine).get());

    // 输出匹配结果
    llvm::outs() << std::format("Number of matches: {}\n", matchCallback.count);

    // 输出错误信息统计
    llvm::outs() << std::format("Number of errors: {}\n", diagEngine.getErrorCount());
    llvm::outs() << "Error messages:\n";
    for (const auto& message : diagEngine.getErrorMessages()) {
        llvm::outs() << message << "\n";
    }

	llvm::outs() << std::format("number of matches: {}\n",
	  matchCallback.count);
	return !status ? 0 : 1;
}

/*
目前进度是把大概的框架给铺好了
问题：
1. VariantValue.h这个狗屁文件死活找不到
2. 关于diagnostic的输出应该规范成什么样的格式？目前的思路是以简洁通用的方式进行输出
3. 具体要处理检查的情况是什么呢？dead store， unreachable code， uninitialized variable， loop invariant，要进一步具体要检查的例子。


10.21更新：
1. 尝试搭建diagnostic engine类，目前找到了一些思路但还不确定能不能实现。下面记录一下目前的思路
diagnostic类的功能：
diagnostic engine类的主要功能是用于处理诊断信息，包括错误和警告的统计，以及错误和警告信息的存储
diagnostic engine类继承自clang::DiagnosticConsumer，重写HandleDiagnostic方法，用于处理诊断信息
HandleDiagnostic方法中，根据诊断信息的级别（Error，Warning，Fatal）进行统计，以及将诊断信息存储到对应的容器中

然后想要使用这个类需要把它给注册到compilerinstance里去，因为ASTMatcher本身并没有接口去注册CompilerInstance。
目前的思路是在MyFrontendAction类中创建一个CustomDiagnosticEngine的实例，然后在CreateASTConsumer方法中将这个实例传递给MyASTConsumer类。
之后在MyASTConsumer类中调用HandleTranslationUnit方法时，将CustomDiagnosticEngine实例传递给MatchFinder类，但不确定这种思路是否行的通。
如果不行的话，感觉也别注册了直接在里创建一个CustomDiagnosticEngine实例就行了。
*/