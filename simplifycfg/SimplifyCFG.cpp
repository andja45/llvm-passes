#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static bool canMergeBlocks(BasicBlock &BB, BasicBlock *&Pred);
static void simplifyBlockPHIs(BasicBlock &BB);
static void updateSuccessorPHIs(BasicBlock &BB, BasicBlock &Pred);
static void moveInstructions(BasicBlock &From, BasicBlock &To);
static void mergeBlocks(BasicBlock &BB, BasicBlock &Pred);

static bool isTrivialBranchBlock(BasicBlock &BB);
static void updateSuccessorPHIsForTrivialBlock(BasicBlock &BB, ArrayRef<BasicBlock *> Predecessors);
static void redirectPredecessors(BasicBlock &BB, BasicBlock &Successor, ArrayRef<BasicBlock *> Predecessors);

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

static bool mergeBasicBlocks(Function &F) {
    bool Changed = false;
    bool LocalChange = true;

    while (LocalChange) {
        LocalChange = false;

        for (BasicBlock &BB : F) {
            BasicBlock *Pred = nullptr;

            if (!canMergeBlocks(BB, Pred))
                continue;

            mergeBlocks(BB, *Pred);

            Changed = true;
            LocalChange = true;
            break;
        }
    }

    return Changed;
}

static bool removeTrivialBranchBlocks(Function &F) {
    bool Changed = false;
    bool LocalChange = true;

    while (LocalChange) {
        LocalChange = false;

        for (BasicBlock &BB : F) {
            if (!isTrivialBranchBlock(BB))
                continue;

            auto *Branch = cast<BranchInst>(BB.getTerminator());
            BasicBlock *Successor = Branch->getSuccessor(0);

            SmallVector<BasicBlock *, 8> Predecessors;
            for (BasicBlock *Pred : predecessors(&BB))
                Predecessors.push_back(Pred);

            updateSuccessorPHIsForTrivialBlock(BB, Predecessors);
            redirectPredecessors(BB, *Successor, Predecessors);

            BB.dropAllReferences();
            BB.eraseFromParent();

            Changed = true;
            LocalChange = true;
            break;
        }
    }

    return Changed;
}

static bool simplifySinglePredecessorPHIs(Function &F) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
        if (!BB.getSinglePredecessor())
            continue;

        while (auto *Phi = dyn_cast<PHINode>(&BB.front())) {
            Value *IncomingValue = Phi->getIncomingValue(0);

            Phi->replaceAllUsesWith(IncomingValue);
            Phi->eraseFromParent();

            Changed = true;
        }
    }

    return Changed;
}

struct SimplifyCFGPass : PassInfoMixin<SimplifyCFGPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        bool Changed = false;
        Changed |= removeUnreachableBlocks(F);
        Changed |= mergeBasicBlocks(F);
        Changed |= removeTrivialBranchBlocks(F);
        Changed |= simplifySinglePredecessorPHIs(F);

        if (Changed)
            return PreservedAnalyses::none();
    
        return PreservedAnalyses::all();
    }
};

static bool canMergeBlocks(BasicBlock &BB, BasicBlock *&Pred) {
    Pred = BB.getSinglePredecessor();
    if (!Pred || Pred == &BB)
        return false;

    auto *Branch = dyn_cast<BranchInst>(Pred->getTerminator());
    if (!Branch || !Branch->isUnconditional())
        return false;

    return Branch->getSuccessor(0) == &BB;
}

static void mergeBlocks(BasicBlock &BB, BasicBlock &Pred) {
    simplifyBlockPHIs(BB);
    updateSuccessorPHIs(BB, Pred);

    Pred.getTerminator()->eraseFromParent();

    moveInstructions(BB, Pred);

    BB.eraseFromParent();
}

static void simplifyBlockPHIs(BasicBlock &BB) {
    while (auto *Phi = dyn_cast<PHINode>(&BB.front())) {
        Value *IncomingValue = Phi->getIncomingValue(0);
        Phi->replaceAllUsesWith(IncomingValue);
        Phi->eraseFromParent();
    }
}

static void updateSuccessorPHIs(BasicBlock &BB, BasicBlock &Pred) {
    for (BasicBlock *Successor : successors(&BB)) {
        for (PHINode &Phi : Successor->phis())
            Phi.replaceIncomingBlockWith(&BB, &Pred);
    }
}

static void moveInstructions(BasicBlock &From, BasicBlock &To) {
    while (!From.empty()) {
        Instruction &I = From.front();
        I.moveBefore(To, To.end());
    }
}


static bool isTrivialBranchBlock(BasicBlock &BB) {
    if (&BB == &BB.getParent()->getEntryBlock())
        return false;
    if (pred_empty(&BB))
        return false;
    if (BB.size() != 1)
        return false;

    auto *Branch = dyn_cast<BranchInst>(BB.getTerminator());
    if (!Branch || !Branch->isUnconditional())
        return false;

    return Branch->getSuccessor(0) != &BB;
}

static void updateSuccessorPHIsForTrivialBlock(BasicBlock &BB, ArrayRef<BasicBlock *> Predecessors) {
    BasicBlock *Successor = cast<BranchInst>(BB.getTerminator())->getSuccessor(0);

    for (PHINode &Phi : Successor->phis()) {
        int IncomingIndex = Phi.getBasicBlockIndex(&BB);
        if (IncomingIndex < 0)
            continue;

        Value *IncomingValue = Phi.getIncomingValue(static_cast<unsigned>(IncomingIndex));
        Phi.removeIncomingValue(static_cast<unsigned>(IncomingIndex), false);

        for (BasicBlock *Pred : Predecessors)
            Phi.addIncoming(IncomingValue, Pred);
    }
}

static void redirectPredecessors(BasicBlock &BB, BasicBlock &Successor, ArrayRef<BasicBlock *> Predecessors) {
    for (BasicBlock *Pred : Predecessors) {
        Instruction *Terminator = Pred->getTerminator();

        for (unsigned i = 0;i < Terminator->getNumSuccessors();++i) {
            if (Terminator->getSuccessor(i) == &BB)
                Terminator->setSuccessor(i, &Successor);
        }
    }
}

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