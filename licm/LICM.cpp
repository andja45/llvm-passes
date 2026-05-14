#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#define DEBUG_TYPE "licm"

using namespace llvm;

STATISTIC(NumHoisted, "Number of instructions hoisted out of loops");
STATISTIC(NumSunk, "Number of instructions sunk out of loops");
STATISTIC(NumPromoted, "Number of memory locations promoted to registers");

static BasicBlock *ensurePreheader(Loop &L, DominatorTree &DT, LoopInfo &LI) {
    if (BasicBlock *PH = L.getLoopPreheader())
        return PH;
    return InsertPreheaderForLoop(&L, &DT, &LI, nullptr, false);
}

struct LICMPass : PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM,
                          LoopStandardAnalysisResults &AR, LPMUpdater &U) {
        LLVM_DEBUG(dbgs() << "[licm] running on loop: "
                          << L.getName() << "\n");

        BasicBlock *Preheader = ensurePreheader(L, AR.DT, AR.LI);
        if (!Preheader) {
            LLVM_DEBUG(dbgs() << "[licm] skipping loop: could not get preheader\n");
            return PreservedAnalyses::all();
        }

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
