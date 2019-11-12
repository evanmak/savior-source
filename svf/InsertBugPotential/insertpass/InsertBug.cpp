#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/CallSite.h"


//standard C++ headers
#include <fstream>
#include <string>
#include <utility>
#include <map>

using namespace std;
using namespace llvm;


static cl::opt<string> InputFilename("i", cl::desc("Specify file path for the bug number information"), cl::value_desc("infopath"));
//static cl::opt<string> OutputFilename("o", cl::desc("Specify file path for the instrumented LLVM bc"), cl::value_desc("outputpath"));

namespace llvm {

class PassRegistry;
void initializeInsertBugPassPass(PassRegistry&);
}

namespace {

	struct InsertBugPass : public ModulePass {

		static char ID;
		InsertBugPass() : ModulePass(ID) {}

		bool runOnModule(Module &M) override;

		bool doInitialization(Module &M) override;

	};

}


static uint64_t getBBID(llvm::BasicBlock &bb){

    llvm::MDNode* curLocMDNode = 0;
    for (Instruction& inst: bb.getInstList()){
      if(curLocMDNode = inst.getMetadata("afl_cur_loc"))
		break;
    }

    if(!curLocMDNode) {
      return -1;
    }

    if(curLocMDNode->getNumOperands() <= 0) {
      return -1;
    }

    llvm::Value * val = cast<ValueAsMetadata>(curLocMDNode->getOperand(0))->getValue();

    if(!val){
      return -1;
    }
    return cast<ConstantInt>(val)->getZExtValue();
}


static inline bool starts_with(const std::string& str, const std::string& prefix)
{
  if(prefix.length() > str.length()) { return false; }
  return str.substr(0, prefix.length()) == prefix;
}

static inline bool is_llvm_dbg_intrinsic(Instruction& instr)
{
  const bool is_call = instr.getOpcode() == Instruction::Invoke ||
    instr.getOpcode() == Instruction::Call;
  if(!is_call) { return false; }

  CallSite cs(&instr);
  Function* calledFunc = cs.getCalledFunction();

  if (calledFunc != NULL) {
    const bool ret = calledFunc->isIntrinsic() &&
      starts_with(calledFunc->getName().str(), "llvm.");
    return ret;
  } else {
   return false;
  }
}


//inset bug score into basic block
static bool insert_potential_score(llvm::LLVMContext &C,  llvm::BasicBlock &BB, int score){

         for (auto &inst: BB){

                //skip intrinsic instructions
                if(is_llvm_dbg_intrinsic(inst))
                        continue;

                //insert a medatadata
                ConstantInt *CurLoc = ConstantInt::get(IntegerType::getInt32Ty(C), score);

                auto meta_loc = MDNode::get(C, ConstantAsMetadata::get(CurLoc));
                inst.setMetadata("savior_bug_num", meta_loc);

                return true;
        }

        return false;
}

bool InsertBugPass::runOnModule(Module &M){


	//open file to read information

	//insert medatadta

	uint64_t bbid;
	uint64_t bbscore;
	char dem;

 	std::ifstream is(InputFilename);

	string line;
	getline(is, line);

	map<uint64_t, uint64_t> bbScoreMap;


	while(is >> bbid >> dem >> bbscore){
		bbScoreMap[bbid] = bbscore;
	}


	for (auto &F : M){
		for(auto &BB : F){

			uint64_t srcid = getBBID(BB);
			if(srcid == -1)
				continue;
			if(bbScoreMap.find(srcid) != bbScoreMap.end());
				insert_potential_score(M.getContext(), BB, bbScoreMap[srcid]);
		}
	}


	return true;
}

bool InsertBugPass::doInitialization(Module &M)
{
	return true;
}

char InsertBugPass::ID = 0;


INITIALIZE_PASS_BEGIN(InsertBugPass, "simplifycfg", "Simplify the CFG", false, false)
INITIALIZE_PASS_END(InsertBugPass, "simplifycfg", "Simplify the CFG", false, false)

static RegisterPass<InsertBugPass> X("InsertBug", "InsertBug Pass",
		false /* Only looks at CFG */,
		false /* Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerInsertBugPass(const PassManagerBuilder &,
		legacy::PassManagerBase &PM) {
	PM.add(new InsertBugPass());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
		registerInsertBugPass);
