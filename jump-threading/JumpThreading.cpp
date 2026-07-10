#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/CFG.h"

using namespace llvm;

static bool sameComparison(ICmpInst* A, ICmpInst* B) {

    return A->getPredicate() == B->getPredicate() &&
            A->getOperand(0) == B->getOperand(0) &&
            A->getOperand(1) == B->getOperand(1);
}

static bool canThread(BasicBlock* Pred, BasicBlock* BB) {

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

    return sameComparison(PredCmp, CurrentCmp);
}

static void findThreadingOpportunities(Function &F) {

    for(BasicBlock &BB : F) {
        auto *Branch = dyn_cast<BranchInst>(BB.getTerminator());

        if(!Branch || !Branch->isConditional())
            continue;

        for(BasicBlock* Pred : predecessors(&BB)) {

            if(canThread(Pred, &BB)) {

                errs() << "Threading opportunity:\n";
                errs() << " predecessor: ";
                Pred->printAsOperand(errs(), false);
                errs() << "\n";

                errs() << " current: ";
                BB.printAsOperand(errs(), false);
                errs() << "\n\n";
            }
        }
    }
}

struct JumpThreadingPass : PassInfoMixin<JumpThreadingPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

        (void)AM;
        errs() << "Visiting function: " << F.getName() << "\n";

        /*for(BasicBlock &BB : F) {

            Instruction* Term = BB.getTerminator();

            if(BranchInst* BI = dyn_cast<BranchInst>(Term)) {

                //errs() << " Ovo je BranchInst!\n";

                if(BI->isConditional()) {

                    Value* Cond = BI->getCondition();

                    if(ICmpInst* Cmp = dyn_cast<ICmpInst>(Cond)) {
                        ;
                    }

                    for(BasicBlock* Pred : predecessors(&BB)) {

                        errs() << " ";
                        if(Pred->hasName())
                            errs() << Pred->getName();
                        else
                            errs() << "(unnamed)";

                        errs() << "\n";

                        if(canThread(Pred, &BB))
                            errs() << ">>> Threading opportunity found!\n";
                    }
                }
                else
                    ;
                    //errs() << " Unconditional branch\n";

            }

            if(BB.hasName())
                errs() << BB.getName();
            else
                errs() << "(unnamed)";
            
            errs() << "\n";
           
        }*/

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