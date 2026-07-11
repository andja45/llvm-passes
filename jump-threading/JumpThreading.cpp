#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/CFG.h"
#include <vector>

using namespace llvm;

struct ThreadCandidate {
    BasicBlock *Pred;
    BasicBlock *Current;

    BranchInst *PredBranch;
    BranchInst *CurrentBranch;

    bool TakenEdge;
};

static bool sameComparison(ICmpInst* A, ICmpInst* B) {

    if(A->getPredicate() != B->getPredicate())
        return false;

    auto *LoadA = dyn_cast<LoadInst>(A->getOperand(0));
    auto *LoadB = dyn_cast<LoadInst>(B->getOperand(0));

    if(!LoadA || !LoadB)
        return false;

    if(LoadA->getPointerOperand() != LoadB->getPointerOperand())
        return false;

    return A->getOperand(1) == B->getOperand(1);
}

static ICmpInst* getCompare(BasicBlock* BB) {

    auto *Branch = dyn_cast<BranchInst>(BB->getTerminator());

    if(!Branch)
        return nullptr;

    if(!Branch->isConditional())
        return nullptr;

    return dyn_cast<ICmpInst>(Branch->getCondition());
}

static BasicBlock* skipUnconditionalBlock(BasicBlock* BB) {

    auto *Branch = dyn_cast<BranchInst>(BB->getTerminator());

    if(!Branch || Branch->isConditional() || !BB->getSinglePredecessor())
        return BB;

    return BB->getSinglePredecessor();
}

static bool canThread(BasicBlock* Pred, BasicBlock* BB, ThreadCandidate &Candidate) {

    // predecessor

    Pred = skipUnconditionalBlock(Pred);

    ICmpInst* PredCmp = getCompare(Pred);

    if(!PredCmp)
        return false;

    auto *PredBranch = cast<BranchInst>(Pred->getTerminator());

    // current

    ICmpInst *CurrentCmp = getCompare(BB);

    if (!CurrentCmp)
        return false;

    auto *CurrentBranch = cast<BranchInst>(BB->getTerminator());

    if(!sameComparison(PredCmp, CurrentCmp))
        return false;

    Candidate.Pred = Pred;
    Candidate.Current = BB;

    Candidate.PredBranch = PredBranch;
    Candidate.CurrentBranch = CurrentBranch;

    Candidate.TakenEdge = (PredBranch->getSuccessor(0) == BB);

    return true;
}

static void findThreadingOpportunities(Function &F,
    std::vector<ThreadCandidate>& Candidates) {

    for(BasicBlock &BB : F) {
        auto *Branch = dyn_cast<BranchInst>(BB.getTerminator());

        if(!Branch || !Branch->isConditional())
            continue;

        for(BasicBlock* Pred : predecessors(&BB)) {

            ThreadCandidate Candidate;

            if(canThread(Pred, &BB, Candidate))
                Candidates.push_back(Candidate);
        }
    }
}

static bool threadCandidate(ThreadCandidate& C) {

    BasicBlock* Target;

    if(C.TakenEdge)
        Target = C.CurrentBranch->getSuccessor(0);
    else
        Target = C.CurrentBranch->getSuccessor(1);

    errs() << "Threading candidate:\n";

    errs() << " Pred: ";
    C.Pred->printAsOperand(errs(), false);
    errs() << "\n";

    errs() << " Current: ";
    C.Current->printAsOperand(errs(), false);
    errs() << "\n";

    errs() << " Edge: "
        << (C.TakenEdge ? "true" : "false")
        << "\n\n";

    errs() << "Redirecting edge: ";

    C.Pred->printAsOperand(errs(), false);
    errs() << " -> ";
    Target->printAsOperand(errs(), false);

    errs() << "\n";

    if(Target == C.Current || Target == C.Pred)
        return false;

    C.PredBranch->setSuccessor(C.TakenEdge ? 0 : 1, Target);

    return true;
}

static bool processCandidates(std::vector<ThreadCandidate>& Candidates) {

    bool Changed = false;

    for(ThreadCandidate& C : Candidates) {
        Changed |= threadCandidate(C);
    }

    return Changed;
}

struct JumpThreadingPass : PassInfoMixin<JumpThreadingPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

        (void)AM;
        errs() << "Visiting function: " << F.getName() << "\n";

        std::vector<ThreadCandidate> Candidates;

        findThreadingOpportunities(F, Candidates);

        errs() << "Found " << Candidates.size() << " threading candidate(s)\n";

        bool Changed = processCandidates(Candidates);

        if(Changed)
            return PreservedAnalyses::none();
        
        return PreservedAnalyses::all();
    }

};

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {

    return {
        LLVM_PLUGIN_API_VERSION,
        "JumpThreading",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {

            errs() << "Registering plugin\n";

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "my-jump-threading") {
                        FPM.addPass(JumpThreadingPass());
                        return true;
                    }

                    return false;
                }
            );
        }
    };
}