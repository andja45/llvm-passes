#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#define DEBUG_TYPE "licm" // with -debug-only=licm option

using namespace llvm;

// with --stats
STATISTIC(NumHoisted, "Number of instructions hoisted out of loops");
STATISTIC(NumSunk, "Number of instructions sunk out of loops");
STATISTIC(NumPromoted, "Number of memory locations promoted to registers");
STATISTIC(NumSEHoisted, "Number of SE-unlocked instructions hoisted out of loops");
STATISTIC(NumSESunk, "Number of SE-unlocked instructions sunk to loop exits");

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

// avoids O(n^2) rescanning per candidate, collects beforehand in O(n)
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

static bool canHoistCall(CallBase &CB, Loop &L, AAResults &AA, ArrayRef<StoreInst*> LoopStores, ArrayRef<CallBase*> LoopCalls) {
    if (!CB.getCalledFunction())  return false; // function pointer
    if (CB.isInlineAsm())         return false;
    if (CB.isConvergent())        return false; // GPU thread-sensitive
    if (!CB.doesNotThrow())       return false;
    if (!CB.onlyReadsMemory())    return false; // readonly and readnone

    for (Value *Arg : CB.args())
        if (!isLoopInvariant(Arg, L))
            return false;

    // readonly call is unsafe if a loop store writes to memory the call reads
    for (StoreInst *SI : LoopStores)
        if (AA.getModRefInfo(&CB, MemoryLocation::get(SI)) != ModRefInfo::NoModRef)
            return false;

    // readonly call is also unsafe if another call in the loop writes to memory the call reads
    for (CallBase *Other : LoopCalls) {
        if (Other == &CB) continue;
        if (!Other->mayWriteToMemory()) continue;
        if (isModSet(AA.getModRefInfo(Other, &CB))) // checks if Other writes to any location read by CB
            return false;
    }

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
        if (isModSet(AA.getModRefInfo(CB, Loc))) // checks if CB writes to any location read by LI
            return false;

    return true;
}

static bool isSafeToHoist(Instruction &I, Loop &L, DominatorTree &DT) {
    if (I.isTerminator() || isa<PHINode>(I) || isa<LoadInst>(I) || isa<StoreInst>(I) || isa<CallBase>(I))
        return false;

    // strict check - operand must already be outside the loop
    // recursive isLoopInvariant would allow hoisting X that uses Y still in the loop
    for (Value *Op : I.operands())
        if (!L.isLoopInvariant(Op))
            return false;

    // unsafe instructions require dominating all exits which ensures no new execution is introduced
    if (!isSafeToSpeculativelyExecute(&I)) {
        SmallVector<BasicBlock*, 8> ExitBlocks;
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

static BasicBlock *ensurePreheader(Loop &L, DominatorTree &DT, LoopInfo &LI, bool &Inserted) {
    if (BasicBlock *PH = L.getLoopPreheader())
        return PH;
    Inserted = true;
    return InsertPreheaderForLoop(&L, &DT, &LI, nullptr, false);
}

static bool tryPromoteMemory(Loop &L, AAResults &AA, DominatorTree &DT, BasicBlock *Preheader, ArrayRef<LoadInst*> LoopLoads,
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
        if (!LI->isVolatile() && !LI->isAtomic() && isLoopInvariant(LI->getPointerOperand(), L))
            LoadsByPtr[LI->getPointerOperand()].push_back(LI);

    for (StoreInst *SI : LoopStores)
        if (!SI->isVolatile() && !SI->isAtomic() && isLoopInvariant(SI->getPointerOperand(), L))
            StoresByPtr[SI->getPointerOperand()].push_back(SI);

    bool Changed = false;

    // single loop exit, single store per pointer, store block dominates latch
    // these three invariants together guarantee the PHI is correct and the final store reaches memory exactly once
    for (auto &[Ptr, PtrLoads] : LoadsByPtr) {
        auto It = StoresByPtr.find(Ptr);
        if (It == StoresByPtr.end()) continue; // no store (hoisting, not promotion)
        if (It->second.size() != 1) continue; // multiple stores to same address, too complex to promote safely

        StoreInst *StoreToPromote = It->second[0];

        // store must run on every iteration - if the store is in a conditional branch,
        // some iterations skip it and the PHI picks up a stale value from a previous iteration
        if (!DT.dominates(StoreToPromote->getParent(), Latch)) continue;

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

static bool tryReassociateArith(Loop &L, BasicBlock *Preheader) {
    SmallVector<BinaryOperator*, 8> Candidates;

    for (BasicBlock *BB : L.blocks()) {
        for (Instruction &I : *BB) {
            auto *Outer = dyn_cast<BinaryOperator>(&I);
            if (!Outer) continue;

            // only for add and mul (sub and div are not associative, (a-b)-c = a-(b-c) doesn't hold)
            Instruction::BinaryOps OC = Outer->getOpcode();
            if (OC != Instruction::Add  && OC != Instruction::Mul &&
                OC != Instruction::FAdd && OC != Instruction::FMul) continue;

            if (isLoopInvariant(Outer, L)) continue; // whole instruction invariant (regular hoisting handles it)

            Value *Op0 = Outer->getOperand(0), *Op1 = Outer->getOperand(1);

            bool Inv0 = L.isLoopInvariant(Op0), Inv1 = L.isLoopInvariant(Op1);
            if (!Inv0 && !Inv1) continue; // both vary (nothing to extract)

            // varying operand must be the same op ((a+b)+c can regroup, (a+b)*c cannot)
            auto *Inner = dyn_cast<BinaryOperator>(Inv0 ? Op1 : Op0);
            if (!Inner || Inner->getOpcode() != OC) continue;

            // inner op must have a strictly invariant operand to extract to preheader
            if (!L.isLoopInvariant(Inner->getOperand(0)) &&
                !L.isLoopInvariant(Inner->getOperand(1))) continue;

            Candidates.push_back(Outer);
        }
    }

    bool Changed = false;

    for (BinaryOperator *Outer : Candidates) {
        Instruction::BinaryOps OC = Outer->getOpcode();
        Value *Op0 = Outer->getOperand(0), *Op1 = Outer->getOperand(1);
        bool Inv0 = L.isLoopInvariant(Op0);
        Value *OuterInv = Inv0 ? Op0 : Op1;
        Value *OuterVar = Inv0 ? Op1 : Op0;

        auto *Inner = cast<BinaryOperator>(OuterVar); // safe (verified in collection phase)
        Value *InnerInv = L.isLoopInvariant(Inner->getOperand(0))
                              ? Inner->getOperand(0) : Inner->getOperand(1);

        // combine the two invariant sub-expressions once in the preheader (one op instead of two per iteration)
        IRBuilder<> B(Preheader->getTerminator());
        Value *Combined = B.CreateBinOp(OC, InnerInv, OuterInv, "reassoc");

        // rewrite inner op to use the combined invariant (outer op is now redundant)
        Inner->replaceUsesOfWith(InnerInv, Combined);
        Outer->replaceAllUsesWith(Inner);
        Outer->eraseFromParent();

        ++NumHoisted;
        LLVM_DEBUG(dbgs() << "[licm] reassociated: " << *Inner << "\n");
        Changed = true;
    }

    return Changed;
}

static bool tryReassociateGEP(Loop &L, BasicBlock *Preheader) {
    SmallVector<GetElementPtrInst*, 8> Candidates;

    for (BasicBlock *BB : L.blocks()) {
        for (Instruction &I : *BB) {
            auto *GEP = dyn_cast<GetElementPtrInst>(&I);
            if (!GEP) continue;

            if (isLoopInvariant(GEP, L)) continue; // whole GEP invariant (regular hoisting handles it)
            if (!isLoopInvariant(GEP->getPointerOperand(), L)) continue; // varying base (no fixed starting point to hoist to)
            if (GEP->getNumIndices() != 1) continue; // multidimensional (too complex to split)

            // index must be an add with one invariant operand to extract as offset
            auto *Idx = dyn_cast<BinaryOperator>(GEP->getOperand(1));
            if (!Idx || Idx->getOpcode() != Instruction::Add) continue;

            bool InvIdx0 = isLoopInvariant(Idx->getOperand(0), L);
            bool InvIdx1 = isLoopInvariant(Idx->getOperand(1), L);
            if (!InvIdx0 && !InvIdx1) continue; // both vary (nothing to extract)

            Candidates.push_back(GEP);
        }
    }

    bool Changed = false;

    for (GetElementPtrInst *GEP : Candidates) {
        auto *Idx = cast<BinaryOperator>(GEP->getOperand(1)); // safe (verified in collection phase)
        bool InvIdx0 = isLoopInvariant(Idx->getOperand(0), L);
        Value *InvPart = InvIdx0 ? Idx->getOperand(0) : Idx->getOperand(1);
        Value *VarPart = InvIdx0 ? Idx->getOperand(1) : Idx->getOperand(0);

        // advance base pointer by invariant offset once in preheader (stable base for the whole loop)
        IRBuilder<> PreB(Preheader->getTerminator());
        Value *BasePtr = PreB.CreateGEP(GEP->getSourceElementType(), GEP->getPointerOperand(), InvPart, "gep.base");

        // new GEP steps from the preheader base (only the varying index remains)
        IRBuilder<> LoopB(GEP);
        Value *NewGEP = LoopB.CreateGEP(GEP->getSourceElementType(), BasePtr, VarPart, "gep.var");

        // old GEP replaced (NewGEP computes the same address with one fewer op per iteration)
        GEP->replaceAllUsesWith(NewGEP);
        GEP->eraseFromParent();

        ++NumHoisted;
        LLVM_DEBUG(dbgs() << "[licm] GEP reassociated: " << *NewGEP << "\n");
        Changed = true;
    }

    return Changed;
}

static bool tryHoistReciprocal(Loop &L, BasicBlock *Preheader) {
    SmallVector<BinaryOperator*, 8> FDivs;

    for (BasicBlock *BB : L.blocks()) {
        for (Instruction &I : *BB) {
            // only for fdiv (for integer sdiv/udiv x/c = x*(1/c) doesn't hold due to truncation)
            auto *FDiv = dyn_cast<BinaryOperator>(&I);
            if (!FDiv || FDiv->getOpcode() != Instruction::FDiv) continue;

            if (!isLoopInvariant(FDiv->getOperand(1), L)) continue; // condition for reciprocal invariance
            if (isLoopInvariant(FDiv->getOperand(0), L))  continue; // both invariant (regular hoisting handles it)

            FDivs.push_back(FDiv);
        }
    }

    bool Changed = false;

    for (BinaryOperator *FDiv : FDivs) {
        Value *Dividend = FDiv->getOperand(0);
        Value *Divisor  = FDiv->getOperand(1);

        // compute 1.0/divisor once in the preheader (one division for the whole loop)
        IRBuilder<> B(Preheader->getTerminator());
        Value *Recip = B.CreateFDiv(ConstantFP::get(Divisor->getType(), 1.0), Divisor, "recip");

        // new fmul replaces fdiv in place (fmul throughput is up to 40x higher than fdiv)
        Value *FMul = BinaryOperator::CreateFMul(Dividend, Recip, "recip.mul", FDiv);
        FDiv->replaceAllUsesWith(FMul);
        FDiv->eraseFromParent();

        ++NumHoisted;
        LLVM_DEBUG(dbgs() << "[licm] reciprocal hoisted: " << *FMul << "\n");
        Changed = true;
    }

    return Changed;
}

// SE proves loop runs >= 1 time - body instructions isSafeToSpeculativelyExecute rejects (division, faulting loads)
// are safe to hoist (they would have executed anyway on the first iteration)
// dominates-latch check ensures the block runs in every loop case (not in a conditional branch inside the loop)
static bool hoistSEUnlocked(Loop &L, ScalarEvolution &SE, DominatorTree &DT, BasicBlock *Preheader) {
    if (SE.getSmallConstantTripCount(&L) == 0) return false; // we couldn't determine if loop runs with SE

    BasicBlock *Latch = L.getLoopLatch();
    if (!Latch) return false;

    SmallVector<Instruction *, 8> Candidates;
    for (BasicBlock *BB : L.blocks()) {
        if (!DT.dominates(BB, Latch)) continue; // every path from header to latch goes through BB
        for (Instruction &I : *BB) {
            if (I.isTerminator() || isa<PHINode>(I) || isa<LoadInst>(I) ||
                isa<StoreInst>(I) ||  // side effect - n iterations must produce n writes, can't reduce to one
                isa<CallBase>(I)) continue;

            if (isSafeToSpeculativelyExecute(&I)) continue; // regular hoisting handles these
            if (!isLoopInvariant(&I, L)) continue;
            Candidates.push_back(&I);
        }
    }

    for (Instruction *I : Candidates) {
        LLVM_DEBUG(dbgs() << "[licm] se-unlocked hoist: " << *I << "\n");
        hoistInstruction(*I, Preheader); // also increments NumHoisted — SE instructions counted in both totals
        ++NumSEHoisted;
    }

    return !Candidates.empty();
}

static bool trySink(Loop &L, DominatorTree &DT) {
    BasicBlock *ExitBlock = L.getExitBlock();
    if (!ExitBlock) return false; // single exit only

    SmallVector<Instruction*, 8> ToSink;

    for (BasicBlock *BB : L.blocks()) {
        // block must dominate the exit (guarantees it ran on the last iteration)
        if (!DT.dominates(BB, ExitBlock)) continue;

        for (Instruction &I : *BB) {
            if (I.isTerminator() || isa<PHINode>(I)) continue;
            if (I.mayReadOrWriteMemory()) continue; // each store/load is a side effect (sinking skips n-1 of them)

            if (I.use_empty()) continue; // no users to sink toward, DCE will handle it
            bool AllUsersOutside = true;
            for (User *U : I.users()) {
                auto *UI = dyn_cast<Instruction>(U);
                // cannot have users inside the loop
                if (!UI || L.contains(UI->getParent()))
                    { AllUsersOutside = false; break; }
            }
            if (AllUsersOutside) ToSink.push_back(&I);
        }
    }

    bool Changed = false;
    for (Instruction *I : ToSink) {
        I->moveBefore(&*ExitBlock->getFirstInsertionPt());
        ++NumSunk;
        LLVM_DEBUG(dbgs() << "[licm] sunk: " << *I << "\n");
        Changed = true;
    }
    return Changed;
}

static bool sinkSEUnlocked(Loop &L, ScalarEvolution &SE, DominatorTree &DT) {
    if (SE.getSmallConstantTripCount(&L) == 0) return false;

    BasicBlock *ExitBlock = L.getExitBlock();
    if (!ExitBlock) return false;

    BasicBlock *Latch = L.getLoopLatch();
    if (!Latch) return false;

    SmallVector<Instruction*, 8> Candidates;
    for (BasicBlock *BB : L.blocks()) {
        if (!DT.dominates(BB, Latch)) continue; // every path from header to latch goes through BB
        for (Instruction &I : *BB) {
            if (I.isTerminator() || isa<PHINode>(I)) continue;
            if (I.mayReadOrWriteMemory()) continue;

            if (I.use_empty()) continue;
            bool AllUsersOutside = true;
            for (User *U : I.users()) {
                auto *UI = dyn_cast<Instruction>(U);
                if (!UI || L.contains(UI->getParent()))
                    { AllUsersOutside = false; break; }
            }
            if (AllUsersOutside) Candidates.push_back(&I);
        }
    }

    for (Instruction *I : Candidates) {
        LLVM_DEBUG(dbgs() << "[licm] se-unlocked sunk: " << *I << "\n");
        I->moveBefore(&*ExitBlock->getFirstInsertionPt());
        ++NumSunk;
        ++NumSESunk;
    }
    return !Candidates.empty();
}

struct LICMPass : PassInfoMixin<LICMPass> {
    PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &AR, LPMUpdater &U) {
        LLVM_DEBUG(dbgs() << "[licm] running on loop: " << L.getName() << "\n");

        bool PreheaderInserted = false;
        BasicBlock *Preheader = ensurePreheader(L, AR.DT, AR.LI, PreheaderInserted);
        if (!Preheader) {
            LLVM_DEBUG(dbgs() << "[licm] skipping loop: could not get preheader\n");
            return PreservedAnalyses::all();
        }

        bool Changed = false;
        bool Iter;

        // hoisting X can make Y invariant, next iteration hoists Y
        do {
            Iter = false;

            // recollect each iteration (hoisted stores/calls change AA results)
            SmallVector<LoadInst*, 8>  LoopLoads;
            SmallVector<StoreInst*, 8> LoopStores;
            SmallVector<CallBase*, 8>  LoopCalls;
            collectLoopMemoryOps(L, LoopLoads, LoopStores, LoopCalls);

            for (BasicBlock *BB : L.getBlocks()) {
                SmallVector<Instruction*, 8> ToHoist;
                for (Instruction &I : *BB) {
                    if (auto *LI = dyn_cast<LoadInst>(&I)) {
                        if (canHoistLoad(*LI, L, AR.AA, LoopStores, LoopCalls))
                            ToHoist.push_back(&I);
                    } else if (auto *CB = dyn_cast<CallBase>(&I)) {
                        if (canHoistCall(*CB, L, AR.AA, LoopStores, LoopCalls))
                            ToHoist.push_back(&I);
                    } else if (isSafeToHoist(I, L, AR.DT)) {
                        ToHoist.push_back(&I);
                    }
                }
                for (Instruction *I : ToHoist) {
                    hoistInstruction(*I, Preheader);
                    Iter = true;
                }
            }

            Iter |= tryPromoteMemory(L, AR.AA, AR.DT, Preheader, LoopLoads, LoopStores, LoopCalls);
            Iter |= tryReassociateArith(L, Preheader);
            Iter |= tryReassociateGEP(L, Preheader);
            Iter |= tryHoistReciprocal(L, Preheader);
            Iter |= trySink(L, AR.DT);
            Iter |= hoistSEUnlocked(L, AR.SE, AR.DT, Preheader);
            Iter |= sinkSEUnlocked(L, AR.SE, AR.DT);

            Changed |= Iter;
        } while (Iter);

        // fix LCSSA after all transformations (exit PHIs mark where loop-internal values leave the loop)
        formLCSSARecursively(L, AR.DT, &AR.LI, &AR.SE);

        return (Changed || PreheaderInserted) ? PreservedAnalyses::none() : PreservedAnalyses::all();
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
