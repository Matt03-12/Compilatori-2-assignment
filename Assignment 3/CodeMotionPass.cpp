#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/SmallVector.h"
#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace llvm;

namespace {

// ============================================================
//  Dominance Tree (immutato rispetto alla tua versione)
// ============================================================
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

DomTreeMap buildDominanceTree(Function &F)
{
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

  std::map<BasicBlock *, int> RPOIdx;
  for (int i = 0; i < N; i++) RPOIdx[RPO[i]] = i;

  std::vector<int> postOrd(N);
  for (int i = 0; i < N; i++) postOrd[i] = N - 1 - i;

  std::vector<int> idom(N, -1);
  idom[0] = 0;

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


// ============================================================
//  Reaching Definitions (immutato)
// ============================================================
using DefSet   = std::set<Instruction *>;
using ReachMap = std::map<Value *, DefSet>;

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

      ReachMap NewIN;
      for (BasicBlock *Pred : predecessors(BB))
        for (auto &[Val, Defs] : ReachOUT[Pred])
          NewIN[Val].insert(Defs.begin(), Defs.end());

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
    if (&I == V)    Reaching = {&I};
  }

  if (isa<Argument>(V)) Reaching.insert(nullptr);

  return Reaching;
}


// ============================================================
//  Helper LICM: safety check + traversal order
// ============================================================

// FIX (1): un'istruzione è hoistable se
//   (a) è safe-to-speculatively-execute (la sua esecuzione anticipata
//       non può causare trap / observable side effect) OPPURE
//   (b) il suo BB domina TUTTI gli exit block del loop
//       (quindi era garantito che venisse eseguita in ogni run del loop).
static bool isSafeToHoist(Instruction *I, Loop *L, DominatorTree &DT) {
  if (isSafeToSpeculativelyExecute(I))
    return true;

  SmallVector<BasicBlock *, 4> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock *EB : ExitBlocks)
    if (!DT.dominates(I->getParent(), EB))
      return false;
  return true;
}

// Conservativo: il loop contiene istruzioni che scrivono in memoria?
// Usato per decidere se i load possono essere marcati invariant senza
// alias analysis vera.
static bool loopMayWriteMemory(Loop *L) {
  for (BasicBlock *BB : L->blocks())
    for (Instruction &I : *BB)
      if (I.mayWriteToMemory())
        return true;
  return false;
}

// FIX (3): raccolta dei BB del loop in DFS pre-order del dominator tree.
// Garantisce che se A domina B (entrambi nel loop), A appare prima di B.
// Ne consegue che, hoistando in quest'ordine, ogni def viene mossa
// prima di ogni suo use => nessun use-before-def nel preheader.
static void collectLoopBlocksInDomOrder(
    Loop *L, DominatorTree &DT,
    std::vector<BasicBlock *> &Out)
{
  std::vector<DomTreeNode *> Stack;
  Stack.push_back(DT.getNode(L->getHeader()));
  while (!Stack.empty()) {
    DomTreeNode *N = Stack.back();
    Stack.pop_back();
    BasicBlock *BB = N->getBlock();
    if (!L->contains(BB)) continue;
    Out.push_back(BB);
    for (DomTreeNode *Child : *N)
      Stack.push_back(Child);
  }
}


// ============================================================
//  Loop invariant detection (FIX 2: set PER-LOOP)
// ============================================================
using PerLoopInvMap = std::map<Loop *, std::set<Instruction *>>;

void detectLoopInvariant(
    Loop *L,
    std::map<BasicBlock *, ReachMap> &ReachIN,
    PerLoopInvMap &PerLoopInvariants,
    int depth = 0)
{
  std::string Indent(depth * 2, ' ');
  errs() << Indent << "Loop (header: " << L->getHeader()->getName() << ")\n";

  // Bottom-up: prima i sotto-loop. Ognuno ha il proprio set, indipendente.
  for (Loop *SubL : L->getSubLoops())
    detectLoopInvariant(SubL, ReachIN, PerLoopInvariants, depth + 1);

  std::set<BasicBlock *> LoopBlocks(L->block_begin(), L->block_end());
  std::set<Instruction *> &InvariantSet = PerLoopInvariants[L];

  // FIX (4): determino una volta se il loop scrive in memoria.
  // Se sì, niente load nell'invariant set (criterio conservativo
  // in assenza di alias analysis).
  bool LoopWrites = loopMayWriteMemory(L);

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

        // load: solo se nel loop non c'è nessuno store / call con side effect
        if (isa<LoadInst>(I) && LoopWrites) continue;

        bool AllInvariant = true;
        for (Use &U : I.operands()) {
          Value *Op = U.get();

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


// ============================================================
//  Pass principale
// ============================================================
struct TestPass : PassInfoMixin<TestPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    errs() << "Funzione: " << F.getName() << "\n";

    LoopInfo      &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    // --- dominance tree (custom) ---
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

    // --- reaching definitions ---
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

    // --- loop-invariant detection (PER-LOOP) ---
    errs() << "\n=== Loop-Invariant Analysis ===\n";
    PerLoopInvMap PerLoopInvariants;
    if (LI.empty()) {
      errs() << "  No loops found.\n";
    } else {
      for (Loop *L : LI)
        detectLoopInvariant(L, ReachIN, PerLoopInvariants, 0);

      size_t Tot = 0;
      for (auto &P : PerLoopInvariants) Tot += P.second.size();
      errs() << " Istruzioni loop-invariant totali (somma per-loop): "
             << Tot << "\n";
    }

    // --- hoisting (inner-first) con safety check + dom-order traversal ---
    auto Loops = LI.getLoopsInPreorder();
    for (Loop *L : reverse(Loops)) {

      BasicBlock *Preheader = L->getLoopPreheader();
      if (Preheader == nullptr) {
        Preheader = InsertPreheaderForLoop(L, &DT, &LI, nullptr, false);
        // DT è aggiornato da InsertPreheaderForLoop
      }

      auto It = PerLoopInvariants.find(L);
      if (It == PerLoopInvariants.end()) continue;
      std::set<Instruction *> &LInvariants = It->second;
      if (LInvariants.empty()) continue;

      // BB del loop in DFS pre-order del dom tree
      std::vector<BasicBlock *> BBsInOrder;
      collectLoopBlocksInDomOrder(L, DT, BBsInOrder);

      for (BasicBlock *BB : BBsInOrder) {
        SmallVector<Instruction *, 16> ToHoist;
        for (Instruction &I : *BB) {
          if (I.isTerminator())                 continue;
          if (isa<PHINode>(I))                  continue;
          if (I.mayHaveSideEffects())           continue;
          if (!LInvariants.count(&I))           continue;
          if (!isSafeToHoist(&I, L, DT))        continue;   // FIX (1)
          ToHoist.push_back(&I);
        }
        // Sposto: all'interno di un BB, l'ordine di iterazione preserva
        // def-prima-di-use; tra BB diversi lo garantisce il dom-order.
        for (Instruction *I : ToHoist)
          I->moveBefore(Preheader->getTerminator());
      }
    }

    F.print(errs());
    errs() << "---\n";

    // Abbiamo modificato l'IR (moveBefore + eventuale insert preheader):
    // dichiarare 'all()' sarebbe una bugia al PassManager.
    return PreservedAnalyses::none();
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