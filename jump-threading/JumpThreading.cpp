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
};

static bool sameComparison(ICmpInst* A, ICmpInst* B) {

    return A->getPredicate() == B->getPredicate() &&
            A->getOperand(0) == B->getOperand(0) &&
            A->getOperand(1) == B->getOperand(1);
}

static bool canThread(BasicBlock* Pred, BasicBlock* BB, ThreadCandidate &Candidate) {

    // predecessor

    Instruction* PredTerm = Pred->getTerminator();
    auto *PredBranch = dyn_cast<BranchInst>(PredTerm);

    if(!PredBranch)
        return false;

    if(!PredBranch->isConditional())
        return false;

    Value *PredCond = PredBranch->getCondition();

    // current

    auto *CurrentBranch = dyn_cast<BranchInst>(BB->getTerminator());
    
    if(!CurrentBranch)
        return false;

    if(!CurrentBranch->isConditional())
        return false;


    Value *CurrentCond = CurrentBranch->getCondition();

    auto *PredCmp = dyn_cast<ICmpInst>(PredCond);
    auto *CurrentCmp = dyn_cast<ICmpInst>(CurrentCond);

    if(!PredCmp || !CurrentCmp)
        return false;

    if(!sameComparison(PredCmp, CurrentCmp))
        return false;

    Candidate.Pred = Pred;
    Candidate.Current = BB;

    Candidate.PredBranch = PredBranch;
    Candidate.CurrentBranch = CurrentBranch;

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

static void processCandidates(std::vector<ThreadCandidate>& Candidates) {

    for(ThreadCandidate& C : Candidates) {

        errs() << "Threading candidate:\n";

        errs() << " Pred: ";
        C.Pred->printAsOperand(errs(), false);
        errs() << "\n";

        errs() << " Current: ";
        C.Current->printAsOperand(errs(), false);
        errs() << "\n";
    }

}

struct JumpThreadingPass : PassInfoMixin<JumpThreadingPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

        (void)AM;
        errs() << "Visiting function: " << F.getName() << "\n";

        std::vector<ThreadCandidate> Candidates;

        findThreadingOpportunities(F, Candidates);

        errs() << "Found " << Candidates.size() << " threading candidate(s)\n";

        processCandidates(Candidates);

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