#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct SimplifyCFGPass : PassInfoMixin<SimplifyCFGPass> {

    PreservedAnalyses run(
        Function &F,
        FunctionAnalysisManager &FAM) {

        errs() << "Processing function: "
               << F.getName() << "\n";

        return PreservedAnalyses::all();
    }
};

}

PassPluginLibraryInfo getSimplifyCFGPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "SimplifyCFGPass",
        LLVM_VERSION_STRING,

        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "simplifycfg-pass") {
                        FPM.addPass(SimplifyCFGPass{});
                        return true;
                    }

                    return false;
                });
        }};
}

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getSimplifyCFGPluginInfo();
}