#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;
using namespace llvm::PatternMatch;

namespace {

struct MultiInstOptPass : public PassInfoMixin<MultiInstOptPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;
    SmallVector<Instruction*, 8> ToErase;

    for (auto &BB : F) {
      for (auto &I : BB) {

        Value *B;
        ConstantInt *C;

  
        // a = b + C  →  c = a - C
        if (match(&I, m_c_Add(m_Value(B), m_ConstantInt(C)))) {

          SmallVector<User*, 4> Users(I.users());

          for (auto *U : Users) {
            if (auto *Sub = dyn_cast<BinaryOperator>(U)) {

              
              if (Sub->getOpcode() == Instruction::Sub &&
                  Sub->getOperand(0) == &I) {

              if (auto *C2 = dyn_cast<ConstantInt>(Sub->getOperand(1))) {
              if (C2 == C) {

                Sub->replaceAllUsesWith(B);
                ToErase.push_back(Sub);
                Changed = true;
              }
            }
          }
        }
      }
    }
    
        // a = b - C  →  c = a + C
        if (match(&I, m_Sub(m_Value(B), m_ConstantInt(C)))) {

          SmallVector<User*, 4> Users(I.users());

          for (auto *U : Users) {
            if (auto *Add = dyn_cast<BinaryOperator>(U)) {

              if (Add->getOpcode() == Instruction::Add &&
                  match(Add, m_c_Add(m_Specific(&I), m_Specific(C)))) {

                Add->replaceAllUsesWith(B);
                ToErase.push_back(Add);
                Changed = true;
              }
            }
          }
        }

        // a = b * C  →  c = a / C
        if (match(&I, m_c_Mul(m_Value(B), m_ConstantInt(C)))) {

          if (C->isZero())
            continue;

          // controllo overflow
          if (auto *Mul = dyn_cast<BinaryOperator>(&I)) {
            if (Mul->hasNoSignedWrap())
              continue;
          }

          SmallVector<User*, 4> Users(I.users());

          for (auto *U : Users) {
            if (auto *Div = dyn_cast<BinaryOperator>(U)) {

              if (Div->getOpcode() == Instruction::SDiv &&
                  Div->getOperand(0) == &I &&
                  Div->getOperand(1) == C) {

                Div->replaceAllUsesWith(B);
                ToErase.push_back(Div);
                Changed = true;
              }
            }
          }
        }
      }
    }

    for (auto *I : ToErase)
      I->eraseFromParent();

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "multi-inst-opt", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "multi-inst-opt") {
            FPM.addPass(MultiInstOptPass());
            return true;
          }
          return false;
        });
    }
  };
}
