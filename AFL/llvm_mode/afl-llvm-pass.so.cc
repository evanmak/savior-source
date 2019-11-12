/*
  american fuzzy lop - LLVM-mode instrumentation pass
  ---------------------------------------------------
 
  Written by Laszlo Szekeres <lszekeres@google.com> and
  Michal Zalewski <lcamtuf@google.com>

  LLVM integration design comes from Laszlo Szekeres. C bits copied-and-pasted
  from afl-as.c are Michal's fault.

  Copyright 2015, 2016 Google Inc. All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at:

  http://www.apache.org/licenses/LICENSE-2.0

  This library is plugged into LLVM when invoking clang through afl-clang-fast.
  It tells the compiler to add code roughly equivalent to the bits discussed
  in ../afl-as.h.

*/

#define AFL_LLVM_PASS

#include "../config.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

namespace {

  class AFLCoverage : public ModulePass {

  public:

    static char ID;
    AFLCoverage() : ModulePass(ID) { }
    bool seeded = false;
    bool runOnModule(Module &M) override;

    // StringRef getPassName() const override {
    //  return "American Fuzzy Lop Instrumentation";
    // }

  };

}


char AFLCoverage::ID = 0;
static inline bool starts_with(const std::string& str, const std::string& prefix)
{
  if(prefix.length() > str.length()) { return false; }
  return str.substr(0, prefix.length()) == prefix;
}

static inline bool is_sanitizer_handler(BasicBlock& bb)
{
  return starts_with(bb.getName(), "handler.");
}

#include <iostream>
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
    /*if (!isa<Constant>(cs.getCalledValue())){
      llvm::outs() << ">>> inst : \n";
      instr.dump();
      cs.getCalledValue()->getType()->dump();
      }
      Constant* calledValue = cast<Constant>(cs.getCalledValue());
      GlobalValue* globalValue = cast<GlobalValue>(calledValue);
      Function *f = cast<Function>(globalValue);

      const bool ret = f->isIntrinsic() &&
      starts_with(globalValue->getName().str(), "llvm.");
      return ret;*/
    return false;
  }
}

static inline bool
has_sanitizer_instrumentation(BasicBlock &bb){
  bool existSanitizerBr = false;
  for (Instruction& inst : bb.getInstList()) {
    if (inst.getMetadata("afl_edge_sanitizer") != NULL) {
      existSanitizerBr = true;
      break;
    }
  }
  return existSanitizerBr;
}

/* Returns how many lava label were instrumented.*/
static int
insert_lava_label(BasicBlock &bb, LLVMContext &C)
{
  int lava_calls = 0;
  for (auto &ins: bb){
    if (is_llvm_dbg_intrinsic(ins)) continue;
    CallInst* call_inst = dyn_cast<CallInst>(&ins);
    if(call_inst) {
      Function* callee = dyn_cast<Function>(call_inst->getCalledValue());
      if (callee) {
        // OKF("function name %s", callee->getName().str().c_str());
        if (!callee->getName().str().compare("lava_get")) {
          auto meta_lava = MDNode::get(C, llvm::MDString::get(C, "lava"));
          ins.setMetadata("afl_edge_sanitizer", meta_lava);
          lava_calls++;
        }
      }
    }
  }
  return lava_calls;
}

static inline unsigned int
get_block_id(BasicBlock &bb)
{
  unsigned int bbid = 0; 
  MDNode *bb_node = nullptr;
  for (auto &ins: bb){
    if ((bb_node = ins.getMetadata("afl_cur_loc"))) break;
  }
  if (bb_node){
    bbid = cast<ConstantInt>(cast<ValueAsMetadata>(bb_node->getOperand(0))->getValue())->getZExtValue();
  }
  return bbid;
}

static inline unsigned int
get_edge_id(BasicBlock &src, BasicBlock &dst)
{
  unsigned int src_bbid = 0, dst_bbid = 0; 
  src_bbid = get_block_id(src);
  dst_bbid = get_block_id(dst);
  if (src_bbid && dst_bbid){
    return ((src_bbid >> 1) ^ dst_bbid);
  }
  return 0;
}

static inline bool handle_uncov_interesting_inst(Instruction& instr, std::ofstream& outfile)
{
  bool handled = false;
  switch (instr.getOpcode()) {

  case Instruction::Switch:{
    SwitchInst &sw_instr = cast<SwitchInst>(instr);
    auto src_bb = instr.getParent();
    for (unsigned int i = 0; i < sw_instr.getNumSuccessors(); ++i) {
      if (i == 0) outfile << get_block_id(*src_bb)<<":";
      auto dst_bb = sw_instr.getSuccessor(i);
      unsigned int pair_edge = get_edge_id(*src_bb, *dst_bb);
      
      if (pair_edge != 0)
        outfile << pair_edge << " ";
    }
    outfile << "\n"; 
    handled = true;
    break;
    
  }
  case Instruction::Br:{
    BranchInst &br_instr = cast<BranchInst>(instr);
    if (br_instr.isConditional()){
      auto src_bb = instr.getParent();
      for (unsigned int i = 0; i < br_instr.getNumSuccessors(); ++i) {
        if (i == 0) outfile << get_block_id(*src_bb)<<":";
        auto dst_bb = br_instr.getSuccessor(i);
        unsigned int pair_edge = get_edge_id(*src_bb, *dst_bb);
        if (pair_edge != 0)
          outfile << pair_edge << " ";
      }
      outfile << "\n"; 
      handled = true;
    }
    break;
  }

  case Instruction::IndirectBr:{
    IndirectBrInst &ind_br_instr = cast<IndirectBrInst>(instr);
    auto src_bb = instr.getParent();

    for (unsigned int i = 0; i < ind_br_instr.getNumSuccessors(); ++i) {
      if (i == 0) outfile << get_block_id(*src_bb)<<":";
      auto dst_bb = ind_br_instr.getSuccessor(i);
      unsigned int pair_edge = get_edge_id(*src_bb, *dst_bb);
      if (pair_edge != 0)
        outfile << pair_edge << " ";
    }
    outfile << "\n"; 
    handled = true;
    break;
  }

  default:
    handled = false;
  }

  return handled;
}




bool AFLCoverage::runOnModule(Module &M) {
  std::ofstream locMap("locmap.csv", std::ofstream::out);
  std::ofstream labelMap("labelmap.csv", std::ofstream::out);
  std::ofstream pairedEdges("paired_edges.csv", std::ofstream::out);

  if(!seeded) {
    char* random_seed_str = getenv("AFL_RANDOM_SEED");
    if(random_seed_str != NULL) {
      unsigned int seed;
      sscanf(random_seed_str, "%u", &seed);
      srandom(seed);
      //SAYF("seeded with %u\n", seed);
      seeded = true;
    }
  }
  const bool annotate_for_se = (getenv("ANNOTATE_FOR_SE") != NULL);
  LLVMContext &C = M.getContext();

  IntegerType *Int8Ty  = IntegerType::getInt8Ty(C);
  IntegerType *Int32Ty = IntegerType::getInt32Ty(C);
  Type *VoidType = Type::getVoidTy(C);
  PointerType *Ptr8Ty = Type::getInt8PtrTy(C, 0);
  Type* ArgInt32TyInt32TyPtr8Ty[] = {Int32Ty, Int32Ty, Ptr8Ty};
  Type* ArgInt32Ty[] = {Int32Ty};

  /* Show a banner */

  char be_quiet = 0;

  if (isatty(2) && !getenv("AFL_QUIET")) {

    SAYF(cCYA "afl-llvm-pass " cBRI VERSION cRST " by <lszekeres@google.com>\n");

  } else be_quiet = 1;

  /* Decide instrumentation ratio */

  char* inst_ratio_str = getenv("AFL_INST_RATIO");
  unsigned int inst_ratio = 100;

  if (inst_ratio_str) {

    if (sscanf(inst_ratio_str, "%u", &inst_ratio) != 1 || !inst_ratio ||
        inst_ratio > 100)
      FATAL("Bad value of AFL_INST_RATIO (must be between 1 and 100)");

  }

  /* Get globals for the SHM region and the previous location. Note that
     __afl_prev_loc is thread-local. */
  GlobalVariable *AFLMapPtr;
  GlobalVariable *AFLMapBrSanPtr;
  GlobalVariable *AFLPrevLoc;
  Function *AFLLogLoc;
  Function *AFLLogLabel;
  Function *AFLLogTriggered;

  if (!annotate_for_se) {
    AFLMapPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");

    AFLMapBrSanPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_edge_san_area_ptr");

    AFLPrevLoc = new GlobalVariable(
                                    M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
                                    0, GlobalVariable::GeneralDynamicTLSModel, 0, false);

    AFLLogLoc = Function::Create(FunctionType::get(VoidType, ArrayRef<Type*>(ArgInt32TyInt32TyPtr8Ty), false), GlobalValue::ExternalLinkage, "__afl_log_loc", &M);
    AFLLogLabel = Function::Create(FunctionType::get(VoidType, ArrayRef<Type*>(ArgInt32Ty), false), GlobalValue::ExternalLinkage, "__afl_log_label", &M);
    AFLLogTriggered = Function::Create(FunctionType::get(VoidType, ArrayRef<Type*>(ArgInt32Ty),false), GlobalValue::ExternalLinkage, "__afl_log_triggered", &M);
  }

  /* Insert LAVA labels if required.*/
  if (getenv("INSERT_LAVA_LABEL")) {
    int total_lava_num = 0;
#if 0
    // both binary and bc mode instrumentation should contain the metadata.
    // Align with UBSAN labels.
    for (auto &F : M) {
      for (auto &BB : F) {
        total_lava_num += insert_lava_label(BB, C);
      }
    }
    OKF("Instrumented %d lava labels", total_lava_num);
#else
    llvm::Function* lava_get = M.getFunction("lava_get");
    if (lava_get) {
      for (auto& BB : *lava_get) {
        auto meta_lava = MDNode::get(C, llvm::MDString::get(C, "lava"));
        (*BB.getTerminator()).setMetadata("afl_edge_sanitizer", meta_lava);
      }
    }
#endif
  }

  /* Instrument all the things! */

  int inst_blocks = 0;

  for (auto &F : M)
    for (auto &BB : F) {

      BasicBlock::iterator IP = BB.getFirstInsertionPt();
      IRBuilder<> IRB(&(*IP));

      if (AFL_R(100) >= inst_ratio) continue;

      /* Make up cur_loc */

      unsigned int cur_loc = AFL_R(MAP_SIZE);

      ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);

      bool has_savior_label = has_sanitizer_instrumentation(BB);

      auto meta_loc = MDNode::get(C, ConstantAsMetadata::get(CurLoc));
      for (Instruction& instr : BB.getInstList()) {
        if (!is_llvm_dbg_intrinsic(instr)) {
          // this only insert the meta for the first non-llvm dbg
          instr.setMetadata("afl_cur_loc", meta_loc);
          break;
        }
      }
      if (annotate_for_se) {
        bool locMapped = false;
        // auto meta_loc = MDNode::get(C, ConstantAsMetadata::get(CurLoc));
        for (Instruction& instr : BB.getInstList()) {
          if (MDNode* dbg = instr.getMetadata("dbg")) {
            DILocation Loc(dbg);
            locMap << cur_loc << ","
                   << Loc.getDirectory().str() << ","
                   << Loc.getFilename().str() << ","
                   << Loc.getLineNumber() << "\n";
            if (has_savior_label) {
              labelMap << cur_loc << ","
                       << Loc.getFilename().str() << ":"
                       << Loc.getLineNumber() << "\n";
            }
            locMapped = true;
            break;
          }
        }
        if (!locMapped) {
          locMap << cur_loc << ",?,?,0\n";
        }
        // for (Instruction& instr : BB.getInstList()) {
        //   if (!is_llvm_dbg_intrinsic(instr)) {
        //     // this only insert the meta for the first non-llvm dbg
        //     instr.setMetadata("afl_cur_loc", meta_loc);
        //     break;
        //   }
        // }
      } else  {
        /* Load prev_loc */
        LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
        PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
        Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

        /* XOR the cur_loc with prev_loc */
        Value* Xor = IRB.CreateXor(PrevLocCasted, CurLoc);


        /* Load SHM pointer */
        LoadInst *MapPtr = IRB.CreateLoad(AFLMapPtr);
        MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
        Value *MapPtrIdx =
          IRB.CreateGEP(MapPtr, Xor);

        /* log bbid and eid*/
        Value* ArgLoc[] = { CurLoc, Xor, MapPtr};
        IRB.CreateCall(AFLLogLoc, ArrayRef<Value*>(ArgLoc));

        /* Update bitmap */
        LoadInst *Counter = IRB.CreateLoad(MapPtrIdx);
        Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
        Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
        IRB.CreateStore(Incr, MapPtrIdx)
          ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

        if (has_savior_label) {
          /* Load SHM pointer for edge_san region */
          LoadInst *MapBrSanPtr = IRB.CreateLoad(AFLMapBrSanPtr);
          MapBrSanPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
          Value *MapBrSanPtrIdx =
            IRB.CreateGEP(MapBrSanPtr, Xor);

          /* Update bitmap */
          LoadInst *BrCounter = IRB.CreateLoad(MapBrSanPtrIdx);
          BrCounter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
          Value *BrIncr = IRB.CreateAdd(BrCounter, ConstantInt::get(Int8Ty, 1));
          IRB.CreateStore(BrIncr, MapBrSanPtrIdx)
            ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
          /* Log covered label */
          Value* ArgLabel[] = { CurLoc };
          IRB.CreateCall(AFLLogLabel, ArrayRef<Value*>(ArgLabel));
        }
        // The updating the prev_loc will be the same either for SE or for AFL
        /* Set prev_loc to cur_loc >> 1 */
        StoreInst *Store =
          IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
        Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      }

      inst_blocks++;

    }
  
  /*extract paired edges*/
  if (annotate_for_se) {
    for (auto &F : M)
      for (auto &BB : F) {
        Instruction *termInst = dyn_cast<Instruction>(BB.getTerminator());
        if (has_sanitizer_instrumentation(BB))
          continue;
        handle_uncov_interesting_inst(*termInst, pairedEdges);
        // if (handle_uncov_interesting_inst(*termInst, pairedEdges)) {
        //     llvm::outs()<<BB.getTerminator()->getOpcodeName();
        // }
      }
  }else{
    for (auto &F : M)
      for (auto &BB : F) {
        BasicBlock::iterator IP = BB.getFirstInsertionPt();
        IRBuilder<> IRB(&(*IP));
        if (is_sanitizer_handler(BB)) {
          /* Log block id of triggered bugs */
          BasicBlock* pred = BB.getSinglePredecessor();
          if (!pred) {
            WARNF("Sanitizer handler do not have single predecessor."); 
          }else{
            int blk_id = get_block_id(*pred);
            // OKF("parent id: %d", blk_id);
            Value* ArgTriggered[] = {ConstantInt::get(Int32Ty, blk_id)};
            IRB.CreateCall(AFLLogTriggered, ArrayRef<Value*>(ArgTriggered));
          }
        }
      }
  }



  /* Say something nice. */

  if (!be_quiet) {

    if (!inst_blocks) WARNF("No instrumentation targets found.");
    else OKF("Instrumented %u locations (%s mode, ratio %u%%).",
             inst_blocks, getenv("AFL_HARDEN") ? "hardened" :
             ((getenv("AFL_USE_ASAN") || getenv("AFL_USE_MSAN")) ?
              "ASAN/MSAN" : "non-hardened"), inst_ratio);

  }
  locMap.close();
  labelMap.close();
  pairedEdges.close();
  return true;

}


static void registerAFLPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM) {

  PM.add(new AFLCoverage());

}


static RegisterStandardPasses RegisterAFLPass(
                                              PassManagerBuilder::EP_OptimizerLast, registerAFLPass);

static RegisterStandardPasses RegisterAFLPass0(
                                               PassManagerBuilder::EP_EnabledOnOptLevel0, registerAFLPass);
