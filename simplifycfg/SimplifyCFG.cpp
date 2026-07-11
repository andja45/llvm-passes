#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static bool removeUnreachableBlocks(Function &F) {
    
    if (F.empty())
        return false;

    SmallPtrSet<BasicBlock *, 16> ReachableBlocks;
    SmallVector<BasicBlock *, 16> Worklist;

    Worklist.push_back(&F.getEntryBlock());

    while(!Worklist.empty()) {
        BasicBlock *BB = Worklist.pop_back_val();

        if(!ReachableBlocks.insert(BB).second)
            continue;

        for(BasicBlock *Successor : successors(BB))
            Worklist.push_back(Successor);
    }

    SmallVector<BasicBlock *, 8> DeadBlocks;

    for(BasicBlock &BB : F) {
        if(!ReachableBlocks.contains(&BB))
            DeadBlocks.push_back(&BB);
    }

    if(DeadBlocks.empty())
        return false;

    for(BasicBlock *BB : DeadBlocks) {
        for(BasicBlock *Successor : successors(BB)) {
            if(ReachableBlocks.contains(Successor))
                Successor->removePredecessor(BB);
        }
    }

    for(BasicBlock *BB : DeadBlocks)
        BB->dropAllReferences();

    for(BasicBlock *BB : DeadBlocks)
        BB->eraseFromParent();

    return true;
}

struct SimplifyCFGPass : PassInfoMixin<SimplifyCFGPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

        bool Changed = removeUnreachableBlocks(F);
        if (Changed)
            return PreservedAnalyses::none();

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