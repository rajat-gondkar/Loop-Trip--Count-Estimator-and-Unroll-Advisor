#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <string>

using namespace llvm;

namespace {

struct LoopResult {
  std::string Location;
  std::string TripCount;
  std::string Classification;
  std::string Recommendation;
  std::string Rationale;
};

std::string sanitize(std::string Value) {
  for (char &C : Value) {
    if (C == '|' || C == '\n' || C == '\r')
      C = ' ';
  }
  return Value;
}

std::string valueNameOrFallback(const Value *V) {
  if (!V)
    return "<none>";
  if (V->hasName())
    return V->getName().str();

  std::string Buffer;
  raw_string_ostream OS(Buffer);
  V->printAsOperand(OS, false);
  return OS.str();
}

std::string scevToString(const SCEV *Expr) {
  if (!Expr)
    return "<none>";

  std::string Buffer;
  raw_string_ostream OS(Buffer);
  Expr->print(OS);
  return sanitize(OS.str());
}

std::string loopLocation(const Loop &L, StringRef FunctionName) {
  if (BasicBlock *Header = L.getHeader()) {
    for (Instruction &I : *Header) {
      if (const DebugLoc &DL = I.getDebugLoc()) {
        if (const DILocation *Loc = DL.get()) {
          std::string File = Loc->getFilename().str();
          if (File.empty())
            File = Loc->getDirectory().str();
          return File + ":" + std::to_string(Loc->getLine());
        }
      }
    }

    if (Header->hasName())
      return (FunctionName + ":" + Header->getName()).str();
  }

  return (FunctionName + ":<unknown-loop>").str();
}

std::string classifyLoop(const Loop &L, ScalarEvolution &SE, uint64_t &Count) {
  const SCEV *BackedgeCount = SE.getBackedgeTakenCount(&L);
  if (const auto *ConstantCount = dyn_cast<SCEVConstant>(BackedgeCount)) {
    const APInt &Value = ConstantCount->getAPInt();
    if (!Value.isMaxValue()) {
      Count = Value.getZExtValue();
      return "EXACT_STATIC";
    }
  }

  Count = SE.getSmallConstantMaxTripCount(&L);
  if (Count > 0 && Count <= 4096)
    return "BOUNDED_STATIC";

  Count = 0;
  return "DYNAMIC";
}

const PHINode *findInductionVariable(const Loop &L, ScalarEvolution &SE) {
  if (const PHINode *CanonicalIV = L.getInductionVariable(SE))
    return CanonicalIV;

  BasicBlock *Header = L.getHeader();
  if (!Header)
    return nullptr;

  for (const PHINode &Phi : Header->phis()) {
    const SCEV *Expr = SE.getSCEV(const_cast<PHINode *>(&Phi));
    if (const auto *AddRec = dyn_cast<SCEVAddRecExpr>(Expr)) {
      if (AddRec->getLoop() == &L)
        return &Phi;
    }
  }

  return nullptr;
}

void chooseRecommendation(const Loop &L, uint64_t Count, StringRef BaseClass,
                          std::string &Recommendation,
                          std::string &Rationale) {
  const bool IsNestedOuter = !L.getSubLoops().empty();

  if (IsNestedOuter) {
    Recommendation = "do not unroll";
    Rationale = "Nested loop - unrolling outer loop may cause instruction cache pressure";
    return;
  }

  if (BaseClass == "DYNAMIC") {
    Recommendation = "do not unroll";
    Rationale = "Trip count not statically determinable";
    return;
  }

  if (Count <= 8) {
    Recommendation = "unroll fully";
    Rationale = "Small static trip count (" + std::to_string(Count) +
                "), full unroll eliminates loop overhead";
    return;
  }

  if (Count <= 32) {
    Recommendation = "unroll x4";
    Rationale = "Moderate trip count (" + std::to_string(Count) +
                "), x4 unroll balances code size and ILP";
    return;
  }

  Recommendation = "do not unroll";
  Rationale = "Large trip count (" + std::to_string(Count) +
              "), code size increase not justified";
}

LoopResult analyzeLoop(const Loop &L, ScalarEvolution &SE, StringRef FunctionName,
                       unsigned Depth) {
  uint64_t Count = 0;
  std::string BaseClass = classifyLoop(L, SE, Count);
  std::string Classification = BaseClass;
  if (!L.getSubLoops().empty())
    Classification += ",NESTED";

  std::string Recommendation;
  std::string Rationale;
  chooseRecommendation(L, Count, BaseClass, Recommendation, Rationale);

  const PHINode *IV = findInductionVariable(L, SE);
  std::string IVDebug = valueNameOrFallback(IV);
  std::string SCEVDebug = IV ? scevToString(SE.getSCEV(const_cast<PHINode *>(IV)))
                             : "<no canonical induction variable>";

  std::string Location = loopLocation(L, FunctionName);
  if (Depth > 0)
    Location += " depth=" + std::to_string(Depth);

  errs() << "LoopUnrollAdvisor debug | function=" << FunctionName
         << " | location=" << Location << " | depth=" << Depth
         << " | inductionVariable=" << IVDebug << " | scev=" << SCEVDebug
         << "\n";

  return {sanitize(Location),
          BaseClass == "DYNAMIC" ? "-" : std::to_string(Count),
          Classification,
          Recommendation,
          Rationale};
}

void collectLoops(const Loop &L, ScalarEvolution &SE, StringRef FunctionName,
                  unsigned Depth, SmallVectorImpl<LoopResult> &Results) {
  Results.push_back(analyzeLoop(L, SE, FunctionName, Depth));

  for (const Loop *SubLoop : L.getSubLoops())
    collectLoops(*SubLoop, SE, FunctionName, Depth + 1, Results);
}

class LoopUnrollAdvisorPass : public PassInfoMixin<LoopUnrollAdvisorPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    auto &FAMProxy = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M);
    FunctionAnalysisManager &FAM = FAMProxy.getManager();

    SmallVector<LoopResult, 16> Results;

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
      ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);

      for (const Loop *TopLevelLoop : LI)
        collectLoops(*TopLevelLoop, SE, F.getName(), 0, Results);
    }

    outs() << "LOOP_LOCATION | TRIP_COUNT | CLASSIFICATION | RECOMMENDATION | RATIONALE\n";
    for (const LoopResult &R : Results) {
      outs() << R.Location << " | " << R.TripCount << " | " << R.Classification
             << " | " << R.Recommendation << " | " << R.Rationale << "\n";
    }

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopUnrollAdvisor", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop-unroll-advisor") {
                    MPM.addPass(LoopUnrollAdvisorPass());
                    return true;
                  }
                  return false;
                });
          }};
}
