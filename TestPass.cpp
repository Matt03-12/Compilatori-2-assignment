#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include "llvm/IR/Dominators.h"      // DominatorTreeAnalysis, DominatorTree
#include "llvm/Analysis/LoopInfo.h"  // LoopAnalysis, LoopInfo
#include "llvm/Transforms/Utils/LoopUtils.h" // ci serve per la funzione InsertPreheaderForLoop

using namespace llvm;

namespace {

struct TestPass : PassInfoMixin<TestPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    errs() << "Funzione: " << F.getName() << "\n";

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    auto Loops = LI.getLoopsInPreorder();
    for (Loop *L : reverse(Loops))
    { 

    // 0. ottieni preheader (se esiste)
    BasicBlock *Preheader;
      if( (Preheader = L->getLoopPreheader()) == nullptr){
        Preheader = InsertPreheaderForLoop(L, &DT, &LI, nullptr, false);
      }
    
    for (auto &BB : L->getBlocks()) {
    SmallVector<Instruction *, 16> ToHoist;
    while(true){
    for (auto &I : *BB) {
        if (I.isTerminator()) continue;
        if (isa<PHINode>(I)) continue;
        if (I.mayHaveSideEffects()) continue;
        if (!L->hasLoopInvariantOperands(&I)) continue;
        errs() << "l'istruzione sta per essere mossa\n";
        ToHoist.push_back(&I);
    }
    if(ToHoist.empty()){
      break;
    }
    for (auto *I : ToHoist)
        I->moveBefore(Preheader->getTerminator());
      ToHoist.clear();
  }
}

    }
    
    F.print(errs());

    errs() << "---\n";

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "TestPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "test-pass") {
                    FPM.addPass(TestPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getTestPassPluginInfo();
}