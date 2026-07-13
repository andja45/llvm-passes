#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
using namespace llvm;


class DSEPass : public PassInfoMixin<DSEPass> {
private:
    bool hasLoadBetween(StoreInst *OldStore, StoreInst *NewStore) {

        BasicBlock *BB = OldStore->getParent();

        bool between = false;

        for (Instruction &I : *BB) {

            if (&I == OldStore) {
                between = true;
                continue;
            }

            if (&I == NewStore) {
                break;
            }

            if (between) {
                if (auto *Load = dyn_cast<LoadInst>(&I)) {

                    if (Load->getPointerOperand() ==
                        OldStore->getPointerOperand()) {

                        return true;
                        }
                }
            }
        }

        return false;
    }

public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        SmallVector<Instruction *, 16> ToRemove;
        DenseMap<Value*, StoreInst*> LastStore;

        for (BasicBlock &BB : F) {

            for (Instruction &I : BB) {

                if (auto *Store = dyn_cast<StoreInst>(&I)) {

                    Value *Ptr = Store->getPointerOperand();

                    if (LastStore.count(Ptr)) {

                        StoreInst *Previous = LastStore[Ptr];

                        if (!hasLoadBetween(Previous, Store)) {

                            errs() << "Dead store candidate:\n";

                            Previous->print(errs());
                            errs() << "\n";
                            ToRemove.push_back(Previous);
                        } else {

                            errs() << "Store is still needed because of load\n";
                        }
                    }

                    LastStore[Ptr] = Store;
                }
            }
        }

        for (Instruction *I : ToRemove) {
            errs() << "Deleting: ";
            I->print(errs());
            errs() << "\n";
            I->eraseFromParent();
        }

        if (!ToRemove.empty()) {
            return PreservedAnalyses::none();
        }

        return PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "MyDSEPass",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "my-dse") {
                        FPM.addPass(DSEPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}