#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "strength-reduction"

using namespace llvm;

namespace {

struct StrengthReductionPass : PassInfoMixin<StrengthReductionPass> {

  // ---------------------------------------------------------------------------
  //  Genera la catena shl + add che sostituisce la moltiplicazione.
  //  Itera sui bit del moltiplicatore: per ogni bit acceso genera uno shift
  //  e lo accumula con un add.
  // ---------------------------------------------------------------------------
  static Value *emitShiftAddChain(IRBuilder<> &Builder, Value *Base,
                                  unsigned Multiplier, Type *Ty) {

    Value *Acc = nullptr;

    for (unsigned Bit = 0; Bit < 32; ++Bit) {

      if (!((Multiplier >> Bit) & 1))
        continue;

      Value *Shifted =
          (Bit == 0) ? Base
                     : Builder.CreateShl(Base, ConstantInt::get(Ty, Bit),
                                         "shl." + Twine(Bit));

      Acc = Acc ? Builder.CreateAdd(Acc, Shifted, "add." + Twine(Bit))
                : Shifted;
    }

    // Fallback di sicurezza (non dovrebbe mai accadere con Multiplier > 0)
    if (!Acc)
      Acc = ConstantInt::get(Ty, 0);

    return Acc;
  }

  // ---------------------------------------------------------------------------
  //  Entry point del pass
  // ---------------------------------------------------------------------------
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    bool Changed = false;

    LLVM_DEBUG(dbgs() << "[StrengthReduction] analisi di " << F.getName()
                      << "\n");

    for (BasicBlock &BB : F) {
      for (auto It = BB.begin(); It != BB.end();) {

        Instruction &I = *It++;

        if (I.getOpcode() != Instruction::Mul)
          continue;

        // --- Individua operando variabile e costante ------------------------
        Value *Variable = nullptr;
        ConstantInt *CI = nullptr;

        if (auto *C = dyn_cast<ConstantInt>(I.getOperand(1))) {
          Variable = I.getOperand(0);
          CI = C;
        } else if (auto *C = dyn_cast<ConstantInt>(I.getOperand(0))) {
          Variable = I.getOperand(1);
          CI = C;
        }

        if (!CI)
          continue;

        int64_t Multiplier = CI->getSExtValue();
        Type *Ty = I.getType();
        IRBuilder<> Builder(&I);

        LLVM_DEBUG(dbgs() << "  mul: " << I << "  (k = " << Multiplier
                          << ")\n");

        // --- x * 0  →  0 ---------------------------------------------------
        if (Multiplier == 0) {
          I.replaceAllUsesWith(ConstantInt::get(Ty, 0));
          I.eraseFromParent();
          Changed = true;
          continue;
        }

        // --- x * 1  →  x ---------------------------------------------------
        if (Multiplier == 1) {
          I.replaceAllUsesWith(Variable);
          I.eraseFromParent();
          Changed = true;
          continue;
        }

        // --- x * -1  →  0 - x ----------------------------------------------
        if (Multiplier == -1) {
          Value *Neg = Builder.CreateNeg(Variable, "neg");
          I.replaceAllUsesWith(Neg);
          I.eraseFromParent();
          Changed = true;
          continue;
        }

        // --- Gestione segno -------------------------------------------------
        bool IsNegative = (Multiplier < 0);
        unsigned AbsMul =
            static_cast<unsigned>(IsNegative ? -Multiplier : Multiplier);

        // --- Potenza di 2  →  singolo shl ----------------------------------
        if ((AbsMul & (AbsMul - 1)) == 0) {
          unsigned ShiftAmt = 0;
          unsigned Tmp = AbsMul;
          while (Tmp > 1) {
            Tmp >>= 1;
            ShiftAmt++;
          }
          Value *Shifted = Builder.CreateShl(
              Variable, ConstantInt::get(Ty, ShiftAmt), "shl.pow2");

          Value *Result =
              IsNegative ? Builder.CreateNeg(Shifted, "neg") : Shifted;

          I.replaceAllUsesWith(Result);
          I.eraseFromParent();
          Changed = true;
          continue;
        }

        // --- Caso generale: decomposizione in shift + add -------------------
        Value *Result = emitShiftAddChain(Builder, Variable, AbsMul, Ty);

        if (IsNegative)
          Result = Builder.CreateNeg(Result, "neg");

        I.replaceAllUsesWith(Result);
        I.eraseFromParent();
        Changed = true;
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

llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StrengthReductionPass",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "strength-reduction") {
                    FPM.addPass(StrengthReductionPass());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getStrengthReductionPluginInfo();
}
