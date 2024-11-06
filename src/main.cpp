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
#include <clang/Frontend/CompilerInstance.h>
#include "MatchCallback.h"
#include "CheckStrategies.h"
#include "DeadStoresCheck.h"

namespace ct = clang::tooling;
namespace cam = clang::ast_matchers;
namespace lc = llvm::cl;

// Define command line options, accept multiple values
static lc::list<std::string> Checks(
    "checks", lc::desc("Specify checks to run (dead-stores, unreachable-code, uninitialized-variable, loop-invariant)"), 
    lc::ZeroOrMore, // Set the number of values to be zero or more
    lc::value_desc("check"));
static lc::OptionCategory optionCategory("Tool options");
static lc::opt<bool> clAsIs("i", lc::desc("Implicit nodes"),
    lc::cat(optionCategory));

std::unique_ptr<CheckStrategy> getStrategy(const std::string& type) {
    if (type == "dead-stores"){
        return std::make_unique<DeadStoresCheck>("dead-stores");
    } else if(type == "unreachable-code") {
        return nullptr;
    } else if(type == "uninitialized-variable") {
        return nullptr;
    } else if(type == "loop-invariant") {
        return nullptr;
    }else {
        llvm::errs() << "Unknown matcher type: " << type << "\n";
        return nullptr;
    }
}

cam::dynamic::VariantMatcher traverse(clang::TraversalKind kind, cam::dynamic::VariantMatcher matcher) {
    using namespace cam;
    if (matcher.hasTypedMatcher<clang::Decl>()){
        return dynamic::VariantMatcher::SingleMatcher(traverse(kind, matcher.getTypedMatcher<clang::Decl>()));
    }else if (matcher.hasTypedMatcher<clang::Stmt>()) {
        return dynamic::VariantMatcher::SingleMatcher(traverse(kind, matcher.getTypedMatcher<clang::Stmt>()));
    }else{
        llvm::errs() << "Cannot traverse the matcher. No known method to handle it\n";
        return cam::dynamic::VariantMatcher(); 
    }   
}

class MyASTConsumer : public clang::ASTConsumer {
public:
    explicit MyASTConsumer(cam::MatchFinder* Finder) : Finder(Finder) {}

    // After the AST has been parsed completely, the HandleTranslationUnit method is called
    void HandleTranslationUnit(clang::ASTContext &Context) override { Finder->matchAST(Context);}

private:
    cam::MatchFinder* Finder;
};

// Custom FrontendAction
class MyFrontendAction : public clang::ASTFrontendAction {
public:
    MyFrontendAction() 
        : matchFinder(std::make_unique<cam::MatchFinder>()) {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file) override {
        clang::DiagnosticsEngine& diagEngine = CI.getDiagnostics();
        matchCallback = std::make_unique<myproject::MyMatchCallback>(diagEngine);

        // Check the size of Checks
        if (Checks.empty()) {
            unsigned diagID = diagEngine.getCustomDiagID(clang::DiagnosticsEngine::Warning, 
                                                         "No checks specified. At least one check must be provided.");
            diagEngine.Report(diagID);
            return std::make_unique<clang::ASTConsumer>(); // Return an empty ASTConsumer, not sure if this is a best practice
        }

        for (const auto &check : Checks) {
            auto strategy = getStrategy(check);
            if (strategy) {
                for (const auto& matcher : strategy->getMatchers()) {
                    matchFinder->addDynamicMatcher(
                        *traverse(clAsIs ? clang::TK_AsIs : clang::TK_IgnoreUnlessSpelledInSource, matcher).getSingleMatcher(),
                        matchCallback.get()
                    );
                }
                if(matchCallback->AddCheck(std::move(strategy))) llvm::outs() << "Added check: " << check << "\n";
            }
            
            // Log the check and this check cannot go wrong the strategy is already checked. So this is only for log check when adding a "new" check
            //if(matchCallback->AddCheck(check)) llvm::outs() << "Added check: " << check << "\n";
        }

        // Pass the MatchFinder to the ASTConsumer
        return std::make_unique<MyASTConsumer>(matchFinder.get());
    }

    //void EndSourceFileAction() override {}

private:
    std::unique_ptr<myproject::MyMatchCallback> matchCallback;  
    std::unique_ptr<cam::MatchFinder> matchFinder; 
};


int main(int argc, const char **argv) {
	auto optParser = ct::CommonOptionsParser::create(argc, argv, optionCategory);

	if (!optParser) {
		llvm::errs() << llvm::toString(optParser.takeError());
		return 1;
	}

	ct::ClangTool tool(optParser->getCompilations(), optParser->getSourcePathList());

    int status = tool.run(ct::newFrontendActionFactory<MyFrontendAction>().get());
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

10.23更新：
调试好了已有的框架。
1. 关于diagnosticengine的部分，Now try to solve it by using the default setting through FrontendAction class instead of override the HandleDiagnostic method.
And the reason behind this is because the HandleDiagnostic method is not called after overriding(bugs need to be fixed) and the default setting is enough for the diagnostic engine.
2. And i think its pretty important to continue using FrontendAction to access ASTMatcher because u could access CompilerInstance at the same time.
3. It seems to be good for now to have the right way setting up getMatcher and traverse function.
Next Setp:
1. Try to complete the MyMatchCallback by creating a new file to clarify the structure of the callback function.
2. Customize the error message function.

10.27更新：
1. separate the MatchCallback class out of the main.cpp file.
2. Move the "Checks" checker logic to FrontendAction class by using diagnostic engine to report the error message.
3. Add a new logic handling various checkers in the FrontendAction class by using Strategy Pattern. Basically, If want add a new checker, just need to add a new class inherited from CheckStrategy class and implement the getMatchers method.

10.30更新：
1. Refine the old structure by changing the traverse function and adding new method AddCheck to MyMatchCallback class.
2. Add new logic to pass the checkers so that the detailed check logic is implemented in the check method of the CheckStrategy class.

10.31 update:
1. Confirm the structure of the project and start to implement the check logic in the DeadStoresCheck class.
2. Perform analysis on the CFG of the function and get the liveness information of each variable in the function.

Next: Considering using thread pool to speed up the analysis process.

11.4 update:
1. Try inherit the clang::LiveVariables class and override the runOnAllBlocks method to perform the analysis on the CFG of the function.

11.5 update:
1. Finish 80% of the DeadStoresCheck class and the DeadStoreObserver class. Need thorough testing to make sure the logic is correct.

*/