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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Dominators.h"


//standard C++ headers
#include <fstream>
#include <string>
#include <utility>
#include <map>

using namespace std;
using namespace llvm;

// contraint type include a cmp instruction
// and a boolean value indicating whether it should evaluate to true or false.
typedef std::tuple<CmpInst*, bool> Constraint_ty;
typedef std::set<Constraint_ty> Constraints;

static cl::opt<bool> PrintLog("d",
                              cl::desc("print detail of collected constraints"),
                              cl::value_desc("log"),
                              cl::init(false));

namespace llvm {

class PassRegistry;
void initializeFilterDummyPassPass(PassRegistry&);
}

uint64_t g_label_counter = 0;
uint64_t g_total_label_counter = 0;
namespace {

	struct FilterDummyPass : public FunctionPass {

		FilterDummyPass() : FunctionPass(ID) {}

		bool runOnFunction(Function &F) override;

		// bool doInitialization(Module &M) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<DominatorTreeWrapperPass>();
    }

		static char ID;
	};

}

void printSource(Instruction *I);

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

static inline bool is_llvm_ubsan_intrinsic(Instruction& instr)
{
  const bool is_call = instr.getOpcode() == Instruction::Invoke ||
    instr.getOpcode() == Instruction::Call;
  if(!is_call) { return false; }

  CallSite cs(&instr);
  Function* calledFunc = cs.getCalledFunction();

  if (calledFunc != NULL) {
    const bool ret = calledFunc->isIntrinsic() &&
      starts_with(calledFunc->getName().str(), "llvm.") &&
      (calledFunc->getName().str().find(".overflow") != string::npos);
    return ret;
  } else {
    return false;
  }
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

static inline bool isSanitizerBlock(BasicBlock &BB) {
  return BB.getTerminator()->getMetadata("afl_edge_sanitizer") != NULL;
}

//inset bug score into basic block
static bool insert_potential_score(llvm::LLVMContext &C,  llvm::BasicBlock &BB, int score) {

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

#define IS_CONSTANT(c) (dynamic_cast<Constant *>(c) != NULL)
#define IS_INSTRUCTION(i) (dynamic_cast<Instruction *>(i) != NULL)


/*
  Count the operands of instruction by type.
  Return instruction* if there is only one instruction-type operand.
  Otherwise, return NULL.
 */
static Instruction* extractOneVariable(Instruction *I) {
  uint16_t const_ctr = 0;
  uint16_t var_ctr = 0;
  Instruction *res = NULL;

  // Handle PHINode separately
  if (dynamic_cast<PHINode*>(I)) { return NULL;}

  if (!I) { return NULL; }
  for (User::op_iterator oi = I->op_begin(); oi != I->op_end(); ++oi){
    Value *v = *oi;
    if (IS_CONSTANT(v)) {
      const_ctr += 1;
      // llvm::outs()<<"constant operand: " << *v<< "\n";
    } else if (IS_INSTRUCTION(v)) {
      var_ctr += 1;
      res = static_cast<Instruction *>(v);
      // llvm::outs()<<"instruction operand: " << *v<< "\n";
    }else {
      // llvm::outs()<<*v<<"\n";
      // llvm::outs()<<*I<<"\n";
      // llvm::outs() << "encounter pointer type\n";
      // printSource(I);
      return NULL;
      // assert(false && "Encounter unexpected value type");
    }
  }
  return (var_ctr == 1) ? res : NULL;
}

bool isTerminateCondition(Instruction *inst) {
  if (dynamic_cast<CmpInst*>(inst) != NULL) {return true;}
  if (CallInst* call = dynamic_cast<CallInst*>(inst)) {
    if (is_llvm_ubsan_intrinsic(*call)) {
      return true;
    }
  }
  return false;
}

/*
  Follow the use-def chain from the given instruction until
  encounter the termination conditions.
  Early terminate if any instruction in the chain has more
  than one variable.

  Returns the instruction that satisfies the terminate condition.
 */
Instruction* trackUntilTerminations(Instruction *inst) {
  Instruction* res = inst;
  while(!isTerminateCondition(res)) {
    // Only follow condition that has one variable.
    // If encounter complex situation, terminate tracking.
    if(!(res = extractOneVariable(res))){
      // llvm::outs()<<"do not have one and only one variable operand, terminating"
      //             << *inst<<"\n";
      return NULL;
    }
  }
  return res;
}

Instruction* findLoad(Instruction *inst) {
  Instruction* track_var = inst;
  LoadInst* load_inst = NULL;
  while(!(load_inst = dynamic_cast<LoadInst*>(track_var))){
    // llvm::outs() << "tracking: "<< *track_var<<"\n";
    if(!(track_var = extractOneVariable(track_var))){
      return NULL;
    }
  }
  return load_inst;
}

/*
  Track all cmp instructions that satisfy all following conditions
  1) dominate `target`.
  2) applying to the same variable as the target load.
  3) only have one variable operands, the rest are constants.
*/
static Constraints findDominatingCmps(Function &F,
                                             DominatorTree &DT,
                                             Instruction *target){
  Constraints res;
  LoadInst* target_load = dynamic_cast<LoadInst *>(target);
  for (auto &B : F){
    for (auto &I: B) {
      if(dynamic_cast<CmpInst*>(&I) && DT.dominates(&I, target)){
        // llvm::outs() << I << " dominates " << *target<<"\n";
        LoadInst* load_inst = dynamic_cast<LoadInst*>(findLoad(&I));
        if(!load_inst) {continue;}
        if(load_inst->getPointerOperand() == target_load->getPointerOperand()){
          // llvm::outs() << "found: "<<*(load_inst->getPointerOperand())<<"\n";
          if(extractOneVariable(&I)){
            // llvm::outs()<<"we got a winner: "<<I<<"\n";
            CmpInst *candidate = dynamic_cast<CmpInst*>(&I);
            res.insert(std::make_tuple(candidate,false));
          }
        }
      }
    }
  }
  return res;
}

void printSource(Instruction *I){
  if (MDNode* dbg = I->getMetadata("dbg")) {
    DILocation Loc(dbg);
    llvm::outs() << Loc.getDirectory().str() << "/"
                 << Loc.getFilename().str() << ","
                 << Loc.getLineNumber() << "\n";  
  }
}

static bool processLabelBB(BasicBlock &BB, DominatorTree &DT) {
  // llvm::outs()<<"process one BB\n";
  BranchInst *ci = dynamic_cast<BranchInst *>(BB.getTerminator());
  Function *function = BB.getParent();
  assert(ci->isConditional() && "Branch is unconditional.");
  Instruction* cond = dynamic_cast<Instruction*>(ci->getCondition());
  Instruction* track_var = NULL;
  track_var = trackUntilTerminations(cond);
  if (track_var) {
    Instruction* load_var = findLoad(track_var);
    if (load_var) {
      Constraints constraints = findDominatingCmps(*BB.getParent(), DT, load_var);
      if (constraints.size() > 0) {
        if (PrintLog) {
          llvm::outs() << "trigger cond: " << *track_var<< "\n";
          printSource(track_var);
          // llvm::outs() << "origin load: " <<*load_var<<"\n";
          // printSource(load_var);
          llvm::outs() << "constraints: \n";
          for (auto c : constraints){
            llvm::outs() << *std::get<0>(c) << "\n";
            printSource(static_cast<Instruction *>(std::get<0>(c)));
          }
          llvm::outs() << "========================================\n";
        }
        g_label_counter++;
        llvm::outs()<<"remove#: "<<g_label_counter<<"\n";
        llvm::outs()<<"total#: "<<g_total_label_counter<<"\n";
      }
    }
  }
}


bool FilterDummyPass::runOnFunction(Function &F){
  for (auto &BB : F) {
    if (isSanitizerBlock(BB)){
      g_total_label_counter++;
      DominatorTree* DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
      processLabelBB(BB, *DT);
    }
  }
	return false;
}

// bool FilterDummyPass::doInitialization(Function &F)
// {
// 	return true;
// }

char FilterDummyPass::ID = 0;


INITIALIZE_PASS_BEGIN(FilterDummyPass, "filterdummy", "Filter the dummy labels", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(FilterDummyPass, "filterdummy", "Filter the dummy labels", false, false)

static RegisterPass<FilterDummyPass> X("FilterDummy", "FilterDummy Pass",
		false /* Only looks at CFG */,
		false /* Analysis Pass */);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerFilterDummyPass(const PassManagerBuilder &,
		legacy::PassManagerBase &PM) {
	PM.add(new FilterDummyPass());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
		registerFilterDummyPass);
