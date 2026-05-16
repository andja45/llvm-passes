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
#include "llvm/IR/IRBuilder.h"

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
    if (!I || isa<PHINode>(I) || isa<CallBase>(I)) // calls need safety checks beyond operand invariance
        return false;
    for (Value *Op : I->operands())
        if (!isLoopInvariant(Op, L))
            return false;
    return true;
}

// avoids O(n²) rescanning per hoisting candidate, collects beforehand in O(n)
static void collectLoopMemoryOps(Loop &L, SmallVector<LoadInst*, 8> &LoopLoads, SmallVector<StoreInst*, 8> &LoopStores,
                                 SmallVector<CallBase*, 8> &LoopCalls) {
    for (BasicBlock *BB : L.blocks())
        for (Instruction &I : *BB) {
            if (auto *LI = dyn_cast<LoadInst>(&I))
                LoopLoads.push_back(LI);
            else if (auto *SI = dyn_cast<StoreInst>(&I))
                LoopStores.push_back(SI);
            else if (auto *CB = dyn_cast<CallBase>(&I))
                LoopCalls.push_back(CB);
        }
}

static bool canHoistCall(CallBase &CB, Loop &L, AAResults &AA, ArrayRef<StoreInst*> LoopStores) {
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

static bool canHoistLoad(LoadInst &LI, Loop &L, AAResults &AA, ArrayRef<StoreInst*> LoopStores, ArrayRef<CallBase*> LoopCalls) {
    if (LI.isVolatile()) return false;
    if (LI.isAtomic())   return false;

    if (!isLoopInvariant(LI.getPointerOperand(), L)) return false;

    MemoryLocation Loc = MemoryLocation::get(&LI);

    for (StoreInst *SI : LoopStores)
        if (AA.alias(Loc, MemoryLocation::get(SI)) != AliasResult::NoAlias)
            return false;

    for (CallBase *CB : LoopCalls)
        if (AA.getModRefInfo(CB, Loc) != ModRefInfo::NoModRef)
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

static bool tryPromoteMemory(Loop &L, AAResults &AA, BasicBlock *Preheader, ArrayRef<LoadInst*> LoopLoads,
                             ArrayRef<StoreInst*> LoopStores, ArrayRef<CallBase*> LoopCalls) {
    // single exit only (one place to write the promoted value back after the loop)
    BasicBlock *ExitBlock = L.getExitBlock();
    if (!ExitBlock) return false;

    // block that jumps back to the header (for.inc in a for loop)
    BasicBlock *Latch = L.getLoopLatch();
    if (!Latch) return false;

    SmallDenseMap<Value*, SmallVector<LoadInst*, 2>> LoadsByPtr;
    SmallDenseMap<Value*, SmallVector<StoreInst*, 2>> StoresByPtr;

    for (LoadInst *LI : LoopLoads)
        if (!LI->isVolatile() && !LI->isAtomic() &&
            isLoopInvariant(LI->getPointerOperand(), L))
            LoadsByPtr[LI->getPointerOperand()].push_back(LI);

    for (StoreInst *SI : LoopStores)
        if (!SI->isVolatile() && !SI->isAtomic() &&
            isLoopInvariant(SI->getPointerOperand(), L))
            StoresByPtr[SI->getPointerOperand()].push_back(SI);

    bool Changed = false;

    for (auto &[Ptr, PtrLoads] : LoadsByPtr) {
        auto It = StoresByPtr.find(Ptr);
        if (It == StoresByPtr.end()) continue; // no store (hoisting, not promotion)
        if (It->second.size() != 1) continue; // multiple stores to same address, too complex to promote safely

        StoreInst *StoreToPromote = It->second[0];

        // verify no other store or call in the loop aliases this address
        MemoryLocation Loc = MemoryLocation::getBeforeOrAfter(Ptr);
        bool Safe = true;
        for (StoreInst *SI : LoopStores) {
            if (SI == StoreToPromote) continue;
            if (AA.alias(Loc, MemoryLocation::get(SI)) != AliasResult::NoAlias)
                { Safe = false; break; }
        }
        for (CallBase *CB : LoopCalls) {
            if (AA.getModRefInfo(CB, Loc) != ModRefInfo::NoModRef)
                { Safe = false; break; }
        }
        if (!Safe) continue;

        Type *Ty = StoreToPromote->getValueOperand()->getType();

        // load initial value once before the loop starts
        IRBuilder<> PreB(Preheader->getTerminator());
        Value *InitVal = PreB.CreateLoad(Ty, Ptr, "promoted.init");

        // PHI at loop header - first iteration uses InitVal, subsequent iterations use last stored value
        IRBuilder<> HdrB(&*L.getHeader()->begin());
        PHINode *PN = HdrB.CreatePHI(Ty, 2, "promoted");
        PN->addIncoming(InitVal, Preheader);
        PN->addIncoming(StoreToPromote->getValueOperand(), Latch);

        // replace all loads with the PHI (no memory reads inside the loop)
        for (LoadInst *LI : PtrLoads) {
            LI->replaceAllUsesWith(PN);
            LI->eraseFromParent();
        }

        // store removed (PHI carries the value across iterations)
        StoreToPromote->eraseFromParent();

        // write final register value back to memory once at loop exit
        IRBuilder<> ExitB(&*ExitBlock->getFirstInsertionPt());
        ExitB.CreateStore(PN, Ptr);

        ++NumPromoted;
        Changed = true;
    }

    return Changed;
}

struct LICMPass : PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &AR, LPMUpdater &U) {
        LLVM_DEBUG(dbgs() << "[licm] running on loop: " << L.getName() << "\n");

        BasicBlock *Preheader = ensurePreheader(L, AR.DT, AR.LI);
        if (!Preheader) {
            LLVM_DEBUG(dbgs() << "[licm] skipping loop: could not get preheader\n");
            return PreservedAnalyses::all();
        }

        SmallVector<LoadInst*, 8> LoopLoads;
        SmallVector<StoreInst*, 8> LoopStores;
        SmallVector<CallBase*, 8> LoopCalls;
        collectLoopMemoryOps(L, LoopLoads, LoopStores, LoopCalls);

        bool Changed = false;
        for (BasicBlock *BB : L.getBlocks()) {
            SmallVector<Instruction*, 8> ToHoist;
            for (Instruction &I : *BB) {
                if (auto *LI = dyn_cast<LoadInst>(&I)) {
                    if (canHoistLoad(*LI, L, AR.AA, LoopStores, LoopCalls))
                        ToHoist.push_back(&I);
                } else if (auto *CB = dyn_cast<CallBase>(&I)) {
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

        Changed |= tryPromoteMemory(L, AR.AA, Preheader, LoopLoads, LoopStores, LoopCalls);

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
