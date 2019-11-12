//===- DMAPass.cpp -- Dominator Analysis------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
//
//
//===-----------------------------------------------------------------------===//

/*
 * @file: DMAPass.cpp
 * @author: Yueqi Chen, Jun Xu, Yaohui Chen
 * @date: 11/12/2017
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#include "MemoryModel/PointerAnalysis.h"
#include "DMA/DMAPass.h"
#include "WPA/Andersen.h"
#include "WPA/FlowSensitive.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Metadata.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/IR/Type.h>

#include <iostream>
#include <fstream>

using namespace llvm;

char DMAPass::ID = 0;

static RegisterPass<DMAPass> WHOLEPROGRAMPA("dma", "Dominator Analysis");

/// register this into alias analysis group
///static RegisterAnalysisGroup<AliasAnalysis> AA_GROUP(WHOLEPROGRAMPA);

// cl::bits is the class used to represent a list of command line options in the form of a bit vector
// PTATY is pointer analysis type list
// cl::desc attributes specifies a description for the option to be shown in the -help output for the program
static cl::bits<PointerAnalysis::PTATY> PASelected(cl::desc("Select pointer analysis"),
				cl::values(
						clEnumValN(PointerAnalysis::Andersen_WPA, "nander", "Standard inclusion-based analysis"),
						clEnumValN(PointerAnalysis::AndersenLCD_WPA, "lander", "Lazy cycle detection inclusion-based analysis"),
						clEnumValN(PointerAnalysis::AndersenWave_WPA, "wander", "Wave propagation inclusion-based analysis"),
						clEnumValN(PointerAnalysis::AndersenWaveDiff_WPA, "ander", "Diff wave propagation inclusion-based analysis"),
						clEnumValN(PointerAnalysis::FSSPARSE_WPA, "fspta", "Sparse flow sensitive pointer analysis")
						));

static cl::opt<bool> SaviorLabelOnly("savior-label-only", cl::init(false),
                                   cl::desc("Filter basic blocks w/o savior label when returning dominated basic blocks"));

static cl::bits<DMAPass::AliasCheckRule> AliasRule(cl::desc("Select alias check rule"),
				cl::values(
						clEnumValN(DMAPass::Conservative, "conservative", "return MayAlias if any pta says alias"),
						clEnumValN(DMAPass::Veto, "veto", "return NoAlias if any pta says no alias")
						));

static cl::opt<string> FilterFileName("filter", cl::Optional,
                                      cl::desc("Specify file path for the filter file"), cl::value_desc("label filter filepath"));

static cl::opt<string> OutputFilename("o", cl::desc("Specify file path for the dom/reach information"), cl::value_desc("filepath"));

static cl::opt<string> EdgeFilename("edge", cl::desc("Specify file path to store children information"), cl::value_desc("edgefilepath"));

static cl::opt<bool> CountbyDom("dom",
cl::desc("Count labels in dominated code (cound labels in reachable code by default)"),
cl::init(false));

static int64_t getBBID(llvm::BasicBlock *bb);

namespace {

  int CountLabel(const BasicBlock* bb, const char* label) {
    llvm::MDNode* curLocMDNode = 0;
    int num_labels = 0;
    for (const auto& inst: bb->getInstList()){
      if(curLocMDNode = inst.getMetadata(label)) num_labels++;
    }
    return num_labels;
  }

  bool ContainsLabel(const BasicBlock* bb, const char*label) {
    for (const auto& inst: bb->getInstList()){
      if(inst.getMetadata(label)) return true;
    }
    return false;
  }

  SmallVector<const BasicBlock*, 8> FilterSaviorLabel(
                                                      SmallVector<const BasicBlock*, 8> basic_blocks,
                                                      std::set<int64_t>& filter){
    constexpr char kSaviorLabel[] = "afl_edge_sanitizer";
    SmallVector<const BasicBlock*, 8> result;
    for (auto bb : basic_blocks) {
      int num_label = CountLabel(bb, kSaviorLabel);

      if(filter.find(getBBID(const_cast<BasicBlock *>(bb)))
         != filter.end()) {
          // This basic block contains FP label.
          continue;
      }

      for (int i=0; i < num_label; ++i) {
        result.push_back(bb);
      }
    }
    return result;
  }
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

/*!
 * Destructor
 */
DMAPass::~DMAPass() {
		PTAVector::const_iterator it = ptaVector.begin();
		PTAVector::const_iterator eit = ptaVector.end();
		for (; it != eit; ++it) {
				PointerAnalysis* pta = *it;
				delete pta;
		}
		ptaVector.clear();
}

/*!
 * We start from here
 */
bool DMAPass::runOnModule(llvm::Module& module)
{
	/// initialization for llvm alias analyzer
	//InitializeAliasAnalysis(this, SymbolTableInfo::getDataLayout(&module));

	for (u32_t i = 0; i< PointerAnalysis::Default_PTA; i++) {  // check which command line parameter has been set
		if (PASelected.isSet(i))
			runPointerAnalysis(module, i); // run pointer analysis according to parameters
	}

	//if there is not
	if(ptaVector.empty()){
		llvm::outs()<<"Error: No Point-to analysis results. Exit Now!\n";
		exit(1);
	}

	_pta->analyze(module);

	// after pointer to analyze, build a global control flow graph in basic block level
	buildGlobalBBCFG(&module);

	if(CountbyDom){
		// build dominator tree based on the global CFG
		buildGlobalBBDominatorTree();
		outputDomResult(&module, OutputFilename);
	}else{
		outputReachResult(&module, OutputFilename);
	}

	outputOutGoingEdges(&module, EdgeFilename);

	return true;
}

void DMAPass::runPointerAnalysis(llvm::Module& module, u32_t kind)
{
		/// Initialize pointer analysis.
		// _pta is type of PointerAnalysis*
		switch (kind) {
				case PointerAnalysis::Andersen_WPA:
						_pta = new Andersen(); // declared in include/WPA/Andersen.h
						break;
				case PointerAnalysis::AndersenLCD_WPA:
						_pta = new AndersenLCD();
						break;
				case PointerAnalysis::AndersenWave_WPA:
						_pta = new AndersenWave(); // different Andersen algorithm, just more efficient
						break;
				case PointerAnalysis::AndersenWaveDiff_WPA:
						_pta = new AndersenWaveDiff(); // different Andersen algorithm, just more efficient
						break;
				case PointerAnalysis::FSSPARSE_WPA:
						_pta = new FlowSensitive();
						break;
				default:
						llvm::outs()<<"Error: No implementation for this point-to analysis.\n";
						return;
		}
		ptaVector.push_back(_pta); // type of  PTAVector, that is std::vector<PointerAnalysis*>
}


/*!
 * Return alias results based on our points-to/alias analysis
 * TODO: Need to handle PartialAlias and MustAlias here.
 */
llvm::AliasResult DMAPass::alias(const Value* V1, const Value* V2) {

		llvm::AliasResult result = MayAlias;

		PAG* pag = _pta->getPAG();

		/// TODO: When this method is invoked during compiler optimizations, the IR
		///       used for pointer analysis may been changed, so some Values may not
		///       find corresponding PAG node. In this case, we only check alias
		///       between two Values if they both have PAG nodes. Otherwise, MayAlias
		///       will be returned.
		if (pag->hasValueNode(V1) && pag->hasValueNode(V2)) {
				/// Veto is used by default
				if (AliasRule.getBits() == 0 || AliasRule.isSet(Veto)) {
						/// Return NoAlias if any PTA gives NoAlias result
						result = MayAlias;

						for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
										it != eit; ++it) {
								if ((*it)->alias(V1, V2) == NoAlias)
										result = NoAlias;
						}
				}
				else if (AliasRule.isSet(Conservative)) {
						/// Return MayAlias if any PTA gives MayAlias result
						result = NoAlias;

						for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
										it != eit; ++it) {
								if ((*it)->alias(V1, V2) == MayAlias)
										result = MayAlias;
						}
				}
		}

		return result;
}

void DMAPass::buildGlobalBBCFG(llvm::Module* module) {
		globalBBCFG = new GlobalBBCFG(module, _pta->getPTACallGraph());
}

void DMAPass::buildGlobalBBDominatorTree() {
		globalBBDomTree = new GlobalBBDomTree(*globalBBCFG);
}


std::set<int64_t> getFilterBBIdList(std::string filterf) {
    std::set<int64_t> res;
    assert(!filterf.empty());
    int64_t bbid;
    std::ifstream is(filterf);
    while(is >> bbid) {
        res.insert(bbid);
    }
    return res;
}

//output the results into a destnation file
void DMAPass::outputDomResult(llvm::Module* module, std::string OutputFile){

	std::error_code EC;
	raw_ostream *out = &llvm::outs();

	//create a stream output handler
	out	=  new raw_fd_ostream(OutputFile, EC, sys::fs::F_None);

	*out <<"BBID,DOMNUM\n";
	int num_total_bb = 0;
	int num_pruned_bb = 0;

  std::set<int64_t> filter;

  if(!FilterFileName.empty()) {
      filter = getFilterBBIdList(FilterFileName.getValue());
  }

	for (Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {

		int64_t bbid = -1;

		for (Function::iterator B = F->begin(), E = F->end(); B != E; ++B) {

			SmallVector<const BasicBlock*, 8> result;
			globalBBDomTree->getDescendants(&*B, result);
			bbid = getBBID(&*B);

			if(bbid == -1)
				continue;

			if (SaviorLabelOnly) {
				// Filters BBL in results without savior label.
				int prev_size = result.size();
				num_total_bb += prev_size;
				result = FilterSaviorLabel(result, filter);
				num_pruned_bb += prev_size - result.size();
			}

			 //insert_potential_score(module->getContext(), *B, result.size());

			*out << bbid << "," << result.size() <<"\n";
		}
	}
	llvm::outs() << "Total BBL size: " << num_total_bb;
	if (SaviorLabelOnly) {
		llvm::outs() << "Pruned BBL size: " << num_pruned_bb;
	}
	if(out != &llvm::outs())
		delete out;

}


unsigned scoreCFGFunc(GlobalBBCFGNode* cfgnode, std::set<int64_t> &filter){

	//count bb number
	if(!SaviorLabelOnly)
		return 1;

	const llvm::BasicBlock* bbnode = cfgnode->getBasicBlock();

  if (filter.find(getBBID(const_cast<BasicBlock*>(bbnode)))
      != filter.end()) return 0;

	//only count labels in bb
  return ContainsLabel(bbnode, "afl_edge_sanitizer") ? 1:0;

}



//output the results by reachability into a destnation file
void DMAPass::outputReachResult(llvm::Module* module, std::string OutputFile){

	std::error_code EC;
	unsigned totalbb;
	unsigned numoflabel;
	int64_t bbid;
  std::set<int64_t> filter;

  if(!FilterFileName.empty()) {
      filter = getFilterBBIdList(FilterFileName.getValue());
  }

	raw_ostream *out = new raw_fd_ostream(OutputFile, EC, sys::fs::F_None);

	*out <<"BBID,DOMNUM\n";

	std::map<GlobalBBCFGNode*, unsigned> scoreMap
      = globalBBCFG->computeBBScore(&scoreCFGFunc, filter);

	for(auto bbscore : scoreMap){
		const llvm::BasicBlock* llvmnode = bbscore.first->getBasicBlock();

		bbid = getBBID(const_cast<llvm::BasicBlock*>(llvmnode));
		if(bbid == -1)
			continue;

		*out << bbid << "," << bbscore.second <<"\n";

	//	insert_potential_score(module->getContext(), *(const_cast<llvm::BasicBlock*>(llvmnode)), bbscore.second);

	}

	delete out;
}

//output the results by reachability into a destnation file
void DMAPass::outputOutGoingEdges(llvm::Module* module, std::string OutputFile){

	std::error_code EC;
	int64_t srcid, dstid;

  if (OutputFile.empty()) return;

	raw_ostream *out = new raw_fd_ostream(OutputFile, EC, sys::fs::F_None);

	for (Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {

		for (Function::iterator B = F->begin(), E = F->end(); B != E; ++B) {

			srcid = getBBID(&*B);

			if(srcid == -1)
				continue;

			std::vector<const llvm::BasicBlock*> dests = globalBBCFG->getOutGoingEdges(&*B);

			if(dests.empty())
				continue;

			*out<< srcid <<":";

			for(auto dest : dests){
			     dstid = getBBID(const_cast<llvm::BasicBlock*>(&*dest));

		             if( dstid == -1)
				continue;

				*out << ((srcid >> 1) ^ dstid) <<" ";
			}
			*out<<"\n";
		}
	}
	delete out;
}

static int64_t getBBID(llvm::BasicBlock *bb){

    llvm::MDNode* curLocMDNode = 0;
    for (Instruction& inst: bb->getInstList()){

      if(curLocMDNode = inst.getMetadata("afl_cur_loc"))break;

    }

    if(!curLocMDNode) {
      // llvm::outs()<<"getBBID Error 1.\n";
      return -1;
    }

    if(curLocMDNode->getNumOperands() <= 0) {

      // llvm::outs()<<"getBBID Error 2.\n";
      return -1;
    }

    llvm::Value * val = 	cast<ValueAsMetadata>(curLocMDNode->getOperand(0))->getValue();

    if(!val){

      return -1;
      // llvm::outs()<<"getBBID Error 3.\n";
    }

    return cast<ConstantInt>(val)->getZExtValue();
}
