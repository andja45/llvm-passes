#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

using namespace llvm;

struct LICMPass : PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM,
                          LoopStandardAnalysisResults &AR, LPMUpdater &U) {
        return PreservedAnalyses::all();
    }
};

llvm::PassPluginLibraryInfo getLICMPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "LICMPass", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, LoopPassManager &LPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "licm-pass") {
                            LPM.addPass(LICMPass{});
                            return true;
                        }
                        return false;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getLICMPluginInfo();
}
