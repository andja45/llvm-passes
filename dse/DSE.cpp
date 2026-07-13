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
    bool hasUseBetween(StoreInst *OldStore, StoreInst *NewStore) {
        BasicBlock *BB = OldStore->getParent();

        bool between = false;
        Value *Ptr = OldStore->getPointerOperand();

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
                        Ptr) {
                        errs() << "Found load between stores\n";
                        return true;
                    }
                }

                if (auto *Call = dyn_cast<CallInst>(&I)) {
                    for (Use &Arg : Call->args()) {
                        if (Arg.get() == Ptr) {
                            errs() << "Pointer passed to function call\n";
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    bool hasUseAfter(StoreInst *Store) {
        Value *Ptr = Store->getPointerOperand();
        bool found = false;

        for (BasicBlock &BB : *Store->getFunction()) {
            for (Instruction &I : BB) {
                if (&I == Store) {
                    found = true;
                    continue;
                }

                if (!found)
                    continue;

                if (auto *Load = dyn_cast<LoadInst>(&I)) {
                    if (Load->getPointerOperand() == Ptr)
                        return true;
                }

                if (auto *Call = dyn_cast<CallInst>(&I)) {
                    for (Use &Arg : Call->args()) {
                        if (Arg.get() == Ptr)
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

                        if (!hasUseBetween(Previous, Store)) {
                            errs() << "Dead store candidate:\n";
                            Previous->print(errs());
                            errs() << "\n";
                            ToRemove.push_back(Previous);
                        } else {
                            errs() << "Store kept because the value is used\n";
                        }
                    }

                    LastStore[Ptr] = Store;
                }

                else if (auto *Call = dyn_cast<CallInst>(&I)) {
                    for (Use &Arg : Call->args()) {
                        Value *Ptr = Arg.get();
                        if (LastStore.count(Ptr)) {
                            errs()
                                << "Clearing stored value because of call: ";

                            Ptr->print(errs());
                            errs() << "\n";

                            LastStore.erase(Ptr);
                        }
                    }
                }
            }
        }

        for (auto &Entry : LastStore) {
            StoreInst *Store = Entry.second;

            if (!hasUseAfter(Store)) {
                errs() << "Dead store at end:\n";
                Store->print(errs());
                errs() << "\n";

                ToRemove.push_back(Store);
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