#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "identity-elimination"

using namespace llvm;

namespace {

struct IdentityEliminationPass : PassInfoMixin<IdentityEliminationPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    bool Changed = false;

    LLVM_DEBUG(dbgs() << "[IdentityElimination] analisi di " << F.getName()
                      << "\n");

    for (BasicBlock &BB : F) {
      for (auto It = BB.begin(); It != BB.end();) {

        Instruction &I = *It++;

        unsigned Opc = I.getOpcode();

        // Consideriamo solo add, sub, shl, or, xor
        // (tutte operazioni che hanno uno 0 come elemento neutro da un lato)
        if (Opc != Instruction::Add && Opc != Instruction::Sub &&
            Opc != Instruction::Shl && Opc != Instruction::Or &&
            Opc != Instruction::Xor && Opc != Instruction::Mul)
          continue;

        // --- Operando destro == 0 -------------------------------------------
        // add x, 0 → x     sub x, 0 → x     shl x, 0 → x
        // or  x, 0 → x     xor x, 0 → x
        if (auto *CI = dyn_cast<ConstantInt>(I.getOperand(1))) {
          if (CI->isZero()) {
            LLVM_DEBUG(dbgs() << "  eliminata: " << I << "\n");
            I.replaceAllUsesWith(I.getOperand(0));
            I.eraseFromParent();
            Changed = true;
            continue;
          }
        }

        // --- Operando sinistro == 0 -----------------------------------------
        // add 0, x → x     or 0, x → x     xor 0, x → x
        // (NON vale per sub e shl: sub 0,x = -x   shl 0,x = 0)
        if (Opc == Instruction::Add || Opc == Instruction::Or ||
            Opc == Instruction::Xor) {
          if (auto *CI = dyn_cast<ConstantInt>(I.getOperand(0))) {
            if (CI->isZero()) {
              LLVM_DEBUG(dbgs() << "  eliminata: " << I << "\n");
              I.replaceAllUsesWith(I.getOperand(1));
              I.eraseFromParent();
              Changed = true;
              continue;
            }
          }
        }

        // --- Mul per 1 (se arriva da qualche altra trasformazione) ----------
        // mul x, 1 → x     mul 1, x → x
        if (Opc == Instruction::Mul) {
          for (unsigned OpIdx = 0; OpIdx < 2; ++OpIdx) {
            if (auto *CI = dyn_cast<ConstantInt>(I.getOperand(OpIdx))) {
              if (CI->isOne()) {
                LLVM_DEBUG(dbgs() << "  eliminata: " << I << "\n");
                I.replaceAllUsesWith(I.getOperand(1 - OpIdx));
                I.eraseFromParent();
                Changed = true;
                break;
              }
            }
          }
        }
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

// =============================================================================
//  Registrazione plugin
// =============================================================================

llvm::PassPluginLibraryInfo getIdentityEliminationPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "IdentityEliminationPass",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "identity-elimination") {
                    FPM.addPass(IdentityEliminationPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getIdentityEliminationPluginInfo();
}
