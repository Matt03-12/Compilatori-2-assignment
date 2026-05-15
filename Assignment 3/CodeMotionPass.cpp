#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace llvm;

namespace {


//  Dominance Tree
//  Nodo del dominance tree: BB, iDom, figli
struct MyDomNode {
  BasicBlock               *BB;
  BasicBlock               *IDom;      // immediate dominator (nullptr per entry)
  std::vector<BasicBlock *> Children; // blocchi di cui questo è l'iDom
};

using DomTreeMap = std::map<BasicBlock *, MyDomNode>;

// Algoritmo di Cooper, Harvey e Kennedy
static int intersect(int b1, int b2,
                     const std::vector<int> &idom,
                     const std::vector<int> &postOrder)
{
  while (b1 != b2) {
    while (postOrder[b1] < postOrder[b2]) b1 = idom[b1];
    while (postOrder[b2] < postOrder[b1]) b2 = idom[b2];
  }
  return b1;
}

// Costruisce il dominance tree della funzione F tramite RPO + worklist.
DomTreeMap buildDominanceTree(Function &F)
{
  // Calcola l'ordine RPO (reverse post-order) del CFG
  std::vector<BasicBlock *> RPO;
  std::set<BasicBlock *>    Visited;

  std::vector<std::pair<BasicBlock *, bool>> Stack;
  Stack.push_back({&F.getEntryBlock(), false});

  while (!Stack.empty()) {
    auto [BB, processed] = Stack.back();
    Stack.pop_back();
    if (processed) { RPO.push_back(BB); continue; }
    if (Visited.count(BB)) continue;
    Visited.insert(BB);
    Stack.push_back({BB, true});
    for (BasicBlock *Succ : successors(BB))
      if (!Visited.count(Succ))
        Stack.push_back({Succ, false});
  }
  std::reverse(RPO.begin(), RPO.end());

  int N = (int)RPO.size();

  // Mappa BB -> indice in RPO
  std::map<BasicBlock *, int> RPOIdx;
  for (int i = 0; i < N; i++) RPOIdx[RPO[i]] = i;

  // postOrd[i] valore post-order di RPO[i] (inverso dell'indice RPO)
  std::vector<int> postOrd(N);
  for (int i = 0; i < N; i++) postOrd[i] = N - 1 - i;

  // idom[i] indice dell'immediate dominator di RPO[i] se -1 = non definito
  std::vector<int> idom(N, -1);
  idom[0] = 0; // l'entry block domina se stesso

  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (int i = 1; i < N; i++) {
      BasicBlock *BB    = RPO[i];
      int         newIdom = -1;
      for (BasicBlock *Pred : predecessors(BB)) {
        if (!RPOIdx.count(Pred)) continue;
        int pIdx = RPOIdx[Pred];
        if (idom[pIdx] == -1)   continue;
        if (newIdom == -1) newIdom = pIdx;
        else               newIdom = intersect(newIdom, pIdx, idom, postOrd);
      }
      if (newIdom != -1 && newIdom != idom[i]) {
        idom[i] = newIdom;
        Changed  = true;
      }
    }
  }

  // Costruisco la DomTreeMap
  DomTreeMap Tree;
  for (BasicBlock &BB : F) Tree[&BB] = {&BB, nullptr, {}};

  for (int i = 0; i < N; i++) {
    BasicBlock *BB = RPO[i];
    if (i == 0) {
      Tree[BB].IDom = nullptr;
    } else {
      BasicBlock *ImmDom  = RPO[idom[i]];
      Tree[BB].IDom       = ImmDom;
      Tree[ImmDom].Children.push_back(BB);
    }
  }

  return Tree;
}

// Stampa ricorsiva del dominance tree
void printDomTree(BasicBlock       *Root,
                  const DomTreeMap &Tree,
                  const LoopInfo   &LI,
                  int               depth)
{
  std::string Prefix(depth * 4, ' ');
  std::string Connector = (depth == 0) ? "" : "|-- ";

  std::string BBName = Root->getName().str();
  if (BBName.empty()) BBName = "<unnamed>";

  Loop       *L       = LI.getLoopFor(Root);
  std::string LoopTag = "";
  if (L) {
    LoopTag = " [profondita loop =" + std::to_string(L->getLoopDepth()) + "]";
    if (L->getHeader() == Root) LoopTag += " <header>";
  }

  errs() << Prefix << Connector << BBName << LoopTag << "\n";

  auto It = Tree.find(Root);
  if (It == Tree.end()) return;

  std::vector<BasicBlock *> SortedChildren = It->second.Children;
  std::sort(SortedChildren.begin(), SortedChildren.end(),
            [](BasicBlock *A, BasicBlock *B) {
              return A->getName() < B->getName();
            });
  for (BasicBlock *Child : SortedChildren)
    printDomTree(Child, Tree, LI, depth + 1);
}


// Reaching Definitions
using DefSet   = std::set<Instruction *>;
using ReachMap = std::map<Value *, DefSet>;

// Calcola le reaching definitions per ogni BasicBlock tramite analisi forward con worklist.
void computeReachingDefs(
    Function &F,
    std::map<BasicBlock *, ReachMap> &ReachIN,
    std::map<BasicBlock *, ReachMap> &ReachOUT)
{
  for (BasicBlock &BB : F) {
    ReachIN[&BB]  = {};
    ReachOUT[&BB] = {};
  }

  std::vector<BasicBlock *> Worklist;
  for (BasicBlock &BB : F) Worklist.push_back(&BB);

  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (BasicBlock *BB : Worklist) {

      // IN[BB] = unione degli OUT di tutti i predecessori
      ReachMap NewIN;
      for (BasicBlock *Pred : predecessors(BB))
        for (auto &[Val, Defs] : ReachOUT[Pred])
          NewIN[Val].insert(Defs.begin(), Defs.end());

      // OUT[BB] = GEN[BB] ∪ (IN[BB] filtrato per KILL[BB])
      // In SSA ogni Value è definito una volta sola, l'ultima def in BB sovrascrive quelle precedenti (gestione non-SSA inclusa).
      ReachMap NewOUT = NewIN;
      for (Instruction &I : *BB)
        if (!I.getType()->isVoidTy())
          NewOUT[&I] = {&I};

      if (NewOUT != ReachOUT[BB]) {
        ReachOUT[BB] = NewOUT;
        ReachIN[BB]  = NewIN;
        Changed = true;
      } else {
        ReachIN[BB] = NewIN;
      }
    }
  }
}

// Restituisce le reaching defs di V nel punto PRIMA di UseI
// (IN del suo BB + defs che precedono UseI nello stesso BB).
DefSet getReachingDefsAtUse(
    Value *V,
    Instruction *UseI,
    std::map<BasicBlock *, ReachMap> &ReachIN)
{
  if (!isa<Instruction>(V) && !isa<Argument>(V)) return {};

  BasicBlock *BB = UseI->getParent();

  DefSet Reaching;
  if (ReachIN.count(BB) && ReachIN[BB].count(V))
    Reaching = ReachIN[BB][V];

  for (Instruction &I : *BB) {
    if (&I == UseI) break;
    if (&I == V)    Reaching = {&I}; // def locale sovrascrive
  }

  // Argomento di funzione: sempre fuori dal loop, usiamo nullptr
  if (isa<Argument>(V)) Reaching.insert(nullptr);

  return Reaching;
}


//  Loop invariant detection
void detectLoopInvariant(
    Loop *L,
    std::map<BasicBlock *, ReachMap> &ReachIN,
    std::set<Instruction *> &InvariantSet,
    int depth = 0)
{
  std::string Indent(depth * 2, ' ');
  errs() << Indent << "Loop (header: " << L->getHeader()->getName() << ")\n";

  std::set<BasicBlock *> LoopBlocks(L->block_begin(), L->block_end());

  // Prima i sottloop (bottom-up), così le loro invarianti sono già nell'InvariantSet quando analizziamo il loop corrente.
  for (Loop *SubL : L->getSubLoops())
    detectLoopInvariant(SubL, ReachIN, InvariantSet, depth + 1);

  // Punto fisso: ripetiamo finché non troviamo nuove invarianti
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (BasicBlock *BB : LoopBlocks) {
      for (Instruction &I : *BB) {
        if (InvariantSet.count(&I))    continue;
        if (I.isTerminator())          continue;
        if (isa<PHINode>(I))           continue;
        if (isa<StoreInst>(I))         continue;
        if (isa<CallBase>(I))          continue;
        if (I.getType()->isVoidTy())   continue;

        bool AllInvariant = true;
        for (Use &U : I.operands()) {
          Value *Op = U.get();

          // Costanti e argomenti sono sempre invarianti
          if (isa<Constant>(Op) || isa<Argument>(Op)) continue;

          DefSet Defs = getReachingDefsAtUse(Op, &I, ReachIN);
          if (Defs.empty()) { AllInvariant = false; break; }

          bool OpInvariant = true;
          for (Instruction *Def : Defs) {
            if (Def == nullptr)                    continue; // argomento
            if (!L->contains(Def->getParent()))    continue; // fuori loop
            if (!InvariantSet.count(Def)) { OpInvariant = false; break; }
          }
          if (!OpInvariant) { AllInvariant = false; break; }
        }

        if (AllInvariant) {
          InvariantSet.insert(&I);
          Changed = true;
          errs() << Indent << "  [Loop invariant] ";
          I.print(errs());
          errs() << "\n";
        }
      }
    }
  }

  // Stampa le istruzioni NON invarianti
  errs() << Indent << "  Istruzioni Non-invariant:\n";
  for (BasicBlock *BB : LoopBlocks) {
    for (Instruction &I : *BB) {
      if (I.isTerminator() || isa<PHINode>(I) || I.getType()->isVoidTy())
        continue;
      if (!InvariantSet.count(&I)) {
        errs() << Indent << "    ";
        I.print(errs());
        errs() << "\n";
      }
    }
  }
}


//  Pass principale
struct TestPass : PassInfoMixin<TestPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    errs() << "Funzione: " << F.getName() << "\n";

    LoopInfo      &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    // dominance tree
    errs() << "\n=== Dominance Tree ===\n";
    DomTreeMap OurDomTree = buildDominanceTree(F);
    printDomTree(&F.getEntryBlock(), OurDomTree, LI, 0);

    errs() << "\n    Immediate Dominators    \n";
    for (BasicBlock &BB : F) {
      auto &Node = OurDomTree[&BB];
      errs() << "  idom(" << BB.getName() << ") = ";
      if (Node.IDom) errs() << Node.IDom->getName();
      else           errs() << "(nessun dom — entry block)";
      errs() << "\n";
    }
    

    // Reaching definitions
    errs() << "\n=== Reaching Definitions ===\n";
    std::map<BasicBlock *, ReachMap> ReachIN, ReachOUT;
    computeReachingDefs(F, ReachIN, ReachOUT);

    for (BasicBlock &BB : F) {
      errs() << "BB '" << BB.getName() << "' IN:\n";
      for (auto &[Val, Defs] : ReachIN[&BB]) {
        errs() << "  ";
        Val->printAsOperand(errs(), false);
        errs() << " <- {";
        bool first = true;
        for (Instruction *D : Defs) {
          if (!first) errs() << ", ";
          if (D) D->printAsOperand(errs(), false);
          else   errs() << "<errore";
          first = false;
        }
        errs() << "}\n";
      }
    }

    //  Loop-invariants detection
    errs() << "\n=== Loop-Invariant Analysis ===\n";
    std::set<Instruction *> InvariantSet;
    if (LI.empty()) {
      errs() << "  No loops found.\n";
    } else {
      for (Loop *L : LI)
        detectLoopInvariant(L, ReachIN, InvariantSet, 0);
      errs() << " Istruzioni loop-invariant: "
             << InvariantSet.size() << "\n";
    }

    auto Loops = LI.getLoopsInPreorder();
    for (Loop *L : reverse(Loops)) {

      // Ottieni il preheader
      BasicBlock *Preheader;
      if ((Preheader = L->getLoopPreheader()) == nullptr)
        Preheader = InsertPreheaderForLoop(L, &DT, &LI, nullptr, false);

      for (auto &BB : L->getBlocks()) {
        SmallVector<Instruction *, 16> ToHoist;
        while (true) {
          for (auto &I : *BB) {
            if (I.isTerminator())                    continue;
            if (isa<PHINode>(I))                     continue;
            if (I.mayHaveSideEffects())              continue;
            if (!InvariantSet.count(&I))             continue;
            //errs() << "l'istruzione sta per essere mossa\n";
            ToHoist.push_back(&I);
          }
          if (ToHoist.empty()) break;
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


// ============================================================
//  REGISTRAZIONE DEL PLUGIN
// ============================================================
llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "TestPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "code-motion-pass") {
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