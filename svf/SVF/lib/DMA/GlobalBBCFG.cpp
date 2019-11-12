#include "DMA/GlobalBBCFG.h"
#include "Util/GraphUtil.h"
#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/SCCIterator.h>

using namespace llvm;


std::vector<std::string> terminatingFunction{ "exit", "abort", "_exit", "_Exit" };


//check if the current basic block terminates (only check until the dest instruction)
bool  GlobalBBCFG::terminatingInst(llvm::Instruction &dest){

	if(!dest.getParent())
		return false;

	for(BasicBlock::iterator inst = dest.getParent()->begin(); inst != dest.getParent()->end(); inst++){

		if((&*inst) == (&dest))
			break;

		if(isa<CallInst>(inst)){

			llvm::Function * func = cast<CallInst>(inst)->getCalledFunction();


			if(func && func->hasName()){
				StringRef name = func->getName();

				if(std::find(terminatingFunction.begin(), terminatingFunction.end(), name) != terminatingFunction.end()){
					return true;
				}
			}


		}
	}

	return false;
}



void GlobalBBCFG::buildGlobalBBCFG(llvm::Module* module) {
		addAllBBCFGNode(module);
		// naive edges means those between basic blocks with preccessor/successor relation
		addAllNaiveBBCFGEdge(module);
		// advanced edges means those generated due to call/return, thread for/join
		addAllAdvancedBBCFGEdge(module);
}

void GlobalBBCFG::addAllBBCFGNode(llvm::Module* module) {
		for (Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {
				for (Function::iterator B = F->begin(), E = F->end(); B != E; ++B) {
						addBBCFGNode(&*B); // add all basic blocks as a node
				}
		}
}

void GlobalBBCFG::addAllNaiveBBCFGEdge(llvm::Module* module) {
		for (Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {
				for (Function::iterator B = F->begin(), E = F->end(); B != E; ++B) {

						//terminating block, skip
						if(terminatingInst(*(B->getTerminator())))
							continue;

						// fina all edges between a specific bb and its successors -- the naive edge

						for (succ_iterator sit = succ_begin(&*B), set = succ_end(&*B); sit != set; ++sit){
									addBBCFGEdge(&*B, *sit, GlobalBBCFGEdge::NaiveEdge);
						}

				}
		}
}

void GlobalBBCFG::addAllAdvancedBBCFGEdge(llvm::Module* module) {
		// add direct call
		for (Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {
				PTACallGraphNode* callGraphNode = ptaCallGraph->getCallGraphNode(&*F); // get callGraphNode
				// for each out edge of callGraphNode
				for (PTACallGraphNode::iterator BE = callGraphNode->OutEdgeBegin(), E = callGraphNode->OutEdgeEnd(); BE != E; ++BE) {
						PTACallGraphEdge* callEdge = dynamic_cast<PTACallGraphEdge*>(*BE);
						for (auto callinst: callEdge->getDirectCalls()){
							//terminating block, skip
							if(terminatingInst(const_cast<llvm::Instruction&>(*callinst)))
								continue;

								const llvm::Function* calleefunc = analysisUtil::getCallee(callinst);
								// some funcs do no have entry block, calleefunc->getEntryBlock return NULL
								// intrinsic function is a function avaible for use in a given programming
								// language which implementation is handled specially by a compiler
								if(!calleefunc || calleefunc->isDeclaration() || calleefunc->isIntrinsic())
										continue;
								addBBCFGEdge(callinst->getParent(), &calleefunc->getEntryBlock(), GlobalBBCFGEdge::CallRetEdge);
						}
				}
		}

		// add indirect call
		PTACallGraph::CallEdgeMap indirectCallMap = ptaCallGraph->getIndCallMap();
		for (auto calledgemap: indirectCallMap) {
				PTACallGraph::FunctionSet calleeFuncSet = calledgemap.second;
				for (auto calleefunc: calleeFuncSet) {

						//terminating block, skip
						if(terminatingInst(const_cast<llvm::Instruction &>(*calledgemap.first.getInstruction())))
							continue;

						if(!calleefunc || calleefunc->isDeclaration() || calleefunc->isIntrinsic())
								continue;
						addBBCFGEdge(calledgemap.first.getParent(), &calleefunc->getEntryBlock(), GlobalBBCFGEdge::CallRetEdge);
				}
		}
}

void GlobalBBCFG::addBBCFGNode(const llvm::BasicBlock* bb) {
		NodeID id = controlFlowGraphNodeNum;
		GlobalBBCFGNode* controlFlowNode = new GlobalBBCFGNode(id, bb, this);
		addGNode(id, controlFlowNode);
		basicBlockToCFGNodeMap[bb] = controlFlowNode;
		controlFlowGraphNodeNum++;
}

void GlobalBBCFG::addBBCFGEdge(const llvm::BasicBlock* src, const llvm::BasicBlock* dst, GlobalBBCFGEdge::CEDGEK kind) {
		GlobalBBCFGNode* srcNode = getCFGNode(src);
		GlobalBBCFGNode* dstNode = getCFGNode(dst);

		if (!hasGraphEdge(srcNode, dstNode, kind)) {
				GlobalBBCFGEdge* edge = new GlobalBBCFGEdge(srcNode, dstNode, kind);
				addEdge(edge);
		}
}

bool GlobalBBCFG::hasGraphEdge(GlobalBBCFGNode* src, GlobalBBCFGNode* dst, GlobalBBCFGEdge::CEDGEK kind) const {
		GlobalBBCFGEdge edge(src, dst, kind);
		GlobalBBCFGEdge* outEdge = src->hasOutgoingEdge(&edge);
		GlobalBBCFGEdge* inEdge = dst->hasIncomingEdge(&edge);
		if (outEdge && inEdge) {
			assert(outEdge == inEdge && "edges not match");
			return true;
		} else
			return false;
}

//collect the set of children BB of a source node
std::vector<const llvm::BasicBlock* > GlobalBBCFG::getOutGoingEdges(const llvm::BasicBlock* src){

	std::vector<const llvm::BasicBlock* > dests;
	GlobalBBCFGNode *cfgnode =  getCFGNode(src);

	for(auto edge : cfgnode->getOutEdges()){
		dests.push_back(edge->getDstNode()->getBasicBlock());
	}

	return dests;
}


//compute the score for every BB
// note: this impl may count duplicated reachale BBs. Another approach is to store the unique id of
// each basic block in the reachmap. But it is costing too much ram.
std::map<GlobalBBCFGNode*, unsigned> GlobalBBCFG::computeBBScore(
                                                                 unsigned (*scorefunc)(GlobalBBCFGNode*,
                                                                                       std::set<int64_t>&),
                                                                 std::set<int64_t> &filter){

	std::map<GlobalBBCFGNode*, unsigned> res;
	std::map<GlobalBBCFGNode*, unsigned> reachmap;

  //store the number of nodes can be reached by this scc in tempreach;
  unsigned tempreach = 0;
  
	//iterate over the scc
	for(scc_iterator<GlobalBBCFG*> SCCI = scc_begin(this), SCCE= scc_end(this); SCCI != SCCE; ++SCCI){

    tempreach = 0;

		for(auto cfgnode : *SCCI){

			//initialize as empty set
			reachmap[cfgnode] = tempreach;
		}

		//add the nodes in the scc
		for(auto cfgnode : *SCCI) {
      if(scorefunc(cfgnode, filter)) {
        //every valid node itself in the scc
        tempreach++;
      }
    }
    for(auto cfgnode : *SCCI) {
			//nodes reachable by the node in the scc
			for(auto edge : cfgnode->getOutEdges()){
				//dest of each edge outgoing from the the node
				GlobalBBCFGNode* tmpcfgnode = edge->getDstNode();

        if(std::find(SCCI->begin(), SCCI->end(), tmpcfgnode) == SCCI->end()) {
            //add extra node's reachable blocks
            tempreach += reachmap[tmpcfgnode];
        }
			}
		}


		//maintain the results for each node
		//node in a scc share the same score
		 for(auto cfgnode : *SCCI){
			// llvm::outs()<<"Processed one node with size "<< tempreach.size() <<"\n";

			// reachmap[cfgnode] = tempreach;
			res[cfgnode] = tempreach;
     }
	}
	return res;
}

void GlobalBBCFG::dump(const std::string& filename) {
		GraphPrinter::WriteGraphToFile(llvm::outs(), filename, this);
}

void GlobalBBCFG::destroy() {
}

namespace llvm{
template<>
struct DOTGraphTraits<GlobalBBCFG*> : public DefaultDOTGraphTraits {
		typedef GlobalBBCFGNode NodeType;
		typedef NodeType::iterator ChildIteratorType;
		DOTGraphTraits(bool isSimple = false) :
				DefaultDOTGraphTraits(isSimple) {
				}

		static std::string getGraphName(GlobalBBCFG* graph) {
				return "Global Basic Block Control Flow Graph";
		}

		static std::string getNodeLabel(GlobalBBCFGNode* node, GlobalBBCFG* graph) {
				return node->getBasicBlock()->getParent()->getName().str() + "\n" + node->getBasicBlock()->getName().str();
		}

		static std::string getNodeAttributes(GlobalBBCFGNode* node, GlobalBBCFG* graph) {
				return "shape=circle";
		}

		template<class EdgeIter>
		static std::string getEdgeAttributes(GlobalBBCFGNode* node, EdgeIter EI, GlobalBBCFG* globalBBCFG) {
				GlobalBBCFGEdge* edge = *(EI.getCurrent());
				assert(edge && "no edge found");
				std::string color;
				if (edge->getEdgeKind() == GlobalBBCFGEdge::NaiveEdge) {
						color = "color=green";
				} else if (edge->getEdgeKind() == GlobalBBCFGEdge::CallRetEdge) {
						color = "color=blue";
				}
				return color;
		}
};
}
