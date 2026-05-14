#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
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

static bool isLoopInvariant(Value *V, Loop &L) {
    if (L.isLoopInvariant(V)) // covers constants and values defined outside the loop
        return true;
    // covers values defined inside the loop whose operands are loop invariant
    auto *I = dyn_cast<Instruction>(V);
    if (!I || isa<PHINode>(I) || isa<CallBase>(I))
        return false;
    for (Value *Op : I->operands())
        if (!isLoopInvariant(Op, L))
            return false;
    return true;
}

// avoids O(n²) rescanning per hoisting candidate, collects beforehand in O(n)
static void collectLoopStoresAndCalls(Loop &L, SmallVector<StoreInst *, 8> &Stores, SmallVector<CallBase *, 8> &Calls) {
    for (BasicBlock *BB : L.blocks())
        for (Instruction &I : *BB) {
            if (auto *SI = dyn_cast<StoreInst>(&I))
                Stores.push_back(SI);
            else if (auto *CB = dyn_cast<CallBase>(&I))
                Calls.push_back(CB);
        }
}

static bool canHoistCall(CallBase &CB, Loop &L, AAResults &AA,
                         ArrayRef<StoreInst *> LoopStores) {
    if (!CB.getCalledFunction())  return false; // function pointer
    if (CB.isInlineAsm())         return false;
    if (CB.isConvergent())        return false; // GPU thread-sensitive
    if (!CB.doesNotThrow())       return false;
    if (!CB.onlyReadsMemory())    return false;

    for (Value *Arg : CB.args())
        if (!isLoopInvariant(Arg, L))
            return false;

    // readonly call is unsafe if a loop store writes to memory the call reads
    for (StoreInst *SI : LoopStores)
        if (AA.getModRefInfo(&CB, MemoryLocation::get(SI)) != ModRefInfo::NoModRef)
            return false;

    return true;
}

static bool isSafeToHoist(Instruction &I, Loop &L, DominatorTree &DT) {
    if (I.isTerminator() || isa<PHINode>(I) || isa<LoadInst>(I) || isa<CallBase>(I)) return false;

    for (Value *Op : I.operands())
        if (!isLoopInvariant(Op, L))
            return false;

    // unsafe instructions require dominating all exits which ensures no new execution is introduced
    if (!isSafeToSpeculativelyExecute(&I)) {
        SmallVector<BasicBlock *, 8> ExitBlocks;
        L.getExitBlocks(ExitBlocks);
        for (BasicBlock *EB : ExitBlocks)
            if (!DT.dominates(I.getParent(), EB))
                return false;
    }

    return true;
}

static void hoistInstruction(Instruction &I, BasicBlock *Preheader) {
    I.moveBefore(Preheader->getTerminator());
    ++NumHoisted;
    LLVM_DEBUG(dbgs() << "[licm] hoisted: " << I << "\n");
}

static BasicBlock *ensurePreheader(Loop &L, DominatorTree &DT, LoopInfo &LI) {
    if (BasicBlock *PH = L.getLoopPreheader())
        return PH;
    return InsertPreheaderForLoop(&L, &DT, &LI, nullptr, false);
}

struct LICMPass : PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM,
                          LoopStandardAnalysisResults &AR, LPMUpdater &U) {
        LLVM_DEBUG(dbgs() << "[licm] running on loop: " << L.getName() << "\n");

        BasicBlock *Preheader = ensurePreheader(L, AR.DT, AR.LI);
        if (!Preheader) {
            LLVM_DEBUG(dbgs() << "[licm] skipping loop: could not get preheader\n");
            return PreservedAnalyses::all();
        }

        SmallVector<StoreInst *, 8> LoopStores;
        SmallVector<CallBase *, 8> LoopCalls;
        collectLoopStoresAndCalls(L, LoopStores, LoopCalls);

        bool Changed = false;
        for (BasicBlock *BB : L.getBlocks()) {
            SmallVector<Instruction *, 8> ToHoist;
            for (Instruction &I : *BB) {
                if (auto *CB = dyn_cast<CallBase>(&I)) {
                    if (canHoistCall(*CB, L, AR.AA, LoopStores))
                        ToHoist.push_back(&I);
                } else if (isSafeToHoist(I, L, AR.DT)) {
                    ToHoist.push_back(&I);
                }
            }

            for (Instruction *I : ToHoist) {
                hoistInstruction(*I, Preheader);
                Changed = true;
            }
        }

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
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
