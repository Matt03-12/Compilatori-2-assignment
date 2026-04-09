#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"

using namespace llvm;

namespace {

struct TestPass : PassInfoMixin<TestPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    F.print(errs());
    {
    for( BasicBlock &BB : F){
      for(auto it = BB.begin(); it != BB.end();){

        Instruction &I = *it++;

        if(I.getOpcode() == Instruction::Mul || I.getOpcode() == Instruction::Add){
          
          errs() << I.getOpcodeName();
          errs() << "\n";

          Value *v0 = I.getOperand(0);
          Value *v1 = I.getOperand(1);

          if(auto ci = dyn_cast<ConstantInt>(v0)){
            ci->print(errs());
          }
          else{
            v0->print(errs());
          }

          errs() << "\n";
          if(auto ci = dyn_cast<ConstantInt>(v1)){
             //errs() << ci->getZExtValue();
             ci->print(errs());
          }
          else{
            v1->print(errs());
          }
          errs() << "\n";
          errs() << "--------------------------------";
          errs() << "\n";

        }

      }
    }
  }

    


/*
    for(BasicBlock &BB : F){

      for(auto it = BB.begin(); it != BB.end();){

        Instruction &I = *it++;
        if(I.getOpcode() == Instruction::Add){
          Value *v = I.getOperand(0);
        }

      }

    }
*/


    for(BasicBlock &BB : F){
      for(auto it = BB.begin(); it != BB.end(); ){
        Instruction &I = *it++;

        if(I.getOpcode() == Instruction::Mul){
          IRBuilder<> builder(&I);

          if(auto ci =  dyn_cast<ConstantInt>(I.getOperand(1))){
            errs() << "entriamo nel loop del divertimento\n";
            int shift = 32;
            int multiplier = ci->getSExtValue();

            if (multiplier == 0) {
              I.replaceAllUsesWith(ConstantInt::get(I.getType(), 0));
              I.eraseFromParent();
              continue;
}

            Value *values[32] = {nullptr};
            for(unsigned int p = 2147483648; p > 0; p/=2){

              //errs() << "wow ";

              shift--;

              if(p > multiplier){
                //printf("%d è troppo stor\n", p);
                continue;
              }

              multiplier -= p;

              Value *v = ConstantInt::get(Type::getInt32Ty(F.getContext()), shift);
              Value* leftShift = builder.CreateShl(I.getOperand(0), v , "left shift --- ");
              values[shift] = leftShift;
              //I.replaceAllUsesWith(leftShift);
              //I.eraseFromParent();
          }

          ////////////////////////////////////////////////////////////////////////////////////////////////

          Value *v = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);

          for(int i = 0; i < 31; i++){
            if(values[i] != NULL){

              Value* add = builder.CreateAdd(v, values[i] , "add bella --- ");
              v = add;
            }

            
            
          }

          I.replaceAllUsesWith(v);
          I.eraseFromParent();

          }

          
        }
      }
    }

    for(BasicBlock &BB : F){

      for(auto it = BB.begin(); it != BB.end(); ){
        
        Instruction &I = *it++;




        if(I.getOpcode() == Instruction::Add || I.getOpcode() == Instruction::Shl){

          auto ci =  dyn_cast<ConstantInt>(I.getOperand(1));

          if(ci != NULL && ci->getSExtValue() == 0){

            I.replaceAllUsesWith(I.getOperand(0));
            I.eraseFromParent();
            continue;
          }

          ci =  dyn_cast<ConstantInt>(I.getOperand(0));

          if(ci != NULL && ci->getSExtValue() == 0 && I.getOpcode() == Instruction::Add){

            I.replaceAllUsesWith(I.getOperand(1));
            I.eraseFromParent();

          }

        }






      }

    }

    F.print(errs());

    /*

    for (BasicBlock &BB : F){

      for (auto it = BB.begin(); it != BB.end();){

        Instruction &I = *it++;

        if(I.getOpcode() == Instruction::Mul){

          
            errs() << "  wow, moltiplicazione trovata! ";
          }

        errs() << "  istruzione: " << I.getOpcodeName() << "\n";


      }

    }
    
    F.print(errs());

    

    // 2. Numero di argomenti (N+* se variadica)
    errs() << "  Argomenti: " << F.arg_size();
    if (F.isVarArg())
      errs() << "+*";
    errs() << "\n";

    // 3. Chiamate a funzione nello stesso modulo
    //    Contiamo le CallInst che chiamano funzioni definite (non solo dichiarate)
    unsigned callCount = 0;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *call = dyn_cast<CallInst>(&I)) {
          Function *callee = call->getCalledFunction();
          // callee != nullptr  -> chiamata diretta (non tramite puntatore)
          // !callee->isDeclaration() -> la funzione ha un body nel modulo
          if (callee && !callee->isDeclaration()) {
            callCount++;
          }
        }
      }
    }
    errs() << "  Chiamate intra-modulo: " << callCount << "\n";

    // 4. Numero di Basic Blocks
    errs() << "  Basic Blocks: " << F.size() << "\n";

    // 5. Numero di istruzioni
    unsigned instrCount = 0;
    for (BasicBlock &BB : F) {
      instrCount += BB.size();
    }
    errs() << "  Istruzioni: " << instrCount << "\n";

    */

    
    errs() << "()()()()()()()\n";

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