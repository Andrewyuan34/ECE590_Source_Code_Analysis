#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <format>

class CustomDiagnosticEngine : public clang::DiagnosticConsumer {
public:
    CustomDiagnosticEngine() : errorCount(0), warningCount(0) {}

    // Override the HandleDiagnostic method to capture diagnostic information (only error and warning counts)
    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel, const clang::Diagnostic &Info) final {
        if (DiagLevel == clang::DiagnosticsEngine::Error || DiagLevel == clang::DiagnosticsEngine::Fatal) {
            errorCount++;
            std::string errorMessage = formatDiagnosticMessage(Info);
            errorMessages.push_back(errorMessage);
        } else if (DiagLevel == clang::DiagnosticsEngine::Warning) {
            warningCount++;
            std::string warningMessage = formatDiagnosticMessage(Info);
            warningMessages.push_back(warningMessage);
        }
    }

    // Function to get the number of errors encountered
    unsigned getErrorCount() const {
        return errorCount;
    }

    // Function to get the number of warnings encountered
    unsigned getWarningCount() const {
        return warningCount;
    }

    // Function to retrieve error messages
    const std::vector<std::string>& getErrorMessages() const {
        return errorMessages;
    }

    // Function to retrieve warning messages
    const std::vector<std::string>& getWarningMessages() const {
        return warningMessages;
    }

private:
    unsigned errorCount;
    unsigned warningCount;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;

    // Helper function to format diagnostic messages
    std::string formatDiagnosticMessage(const clang::Diagnostic &Info) {
        llvm::SmallString<100> diagStr;
        Info.FormatDiagnostic(diagStr);

        clang::SourceLocation loc = Info.getLocation();
        const clang::SourceManager &sourceMgr = Info.getSourceManager();
        if (loc.isValid()) {
            std::string fileName = sourceMgr.getFilename(loc).str();
            unsigned line = sourceMgr.getSpellingLineNumber(loc);
            unsigned col = sourceMgr.getSpellingColumnNumber(loc);
            return std::format("{}:{}:{} - {}", fileName, line, col, diagStr.c_str());
        } else {
            return std::string(diagStr.c_str());
        }
    }
};
