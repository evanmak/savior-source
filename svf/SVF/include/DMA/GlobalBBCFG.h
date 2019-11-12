/*
	Author: Yueqi Chen
 	Data: 11/16/2017
*/

#ifndef _GLOBALBBCFG_H_
#define _GLOBALBBCFG_H_

#include "MemoryModel/GenericGraph.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include "Util/PTACallGraph.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <vector>
#include <map>


class GlobalBBCFGEdge;
class GlobalBBCFGNode;
class GlobalBBCFG;


typedef GenericEdge<GlobalBBCFGNode> GenericBBCFGEdgeTy;

class GlobalBBCFGEdge : public GenericBBCFGEdgeTy {
public:
		// NaiveEdge means edge between basic block and its sucessors
		// CallRetEdge means edge between basic block with callsite and callee basic block
		enum CEDGEK {
				NaiveEdge, CallRetEdge, TDForkEdge, TDJoinEdge, HareParForEdge 
		};

		GlobalBBCFGEdge(GlobalBBCFGNode* s, GlobalBBCFGNode* d, CEDGEK kind): GenericBBCFGEdgeTy(s,d,kind) {
		}

		virtual ~GlobalBBCFGEdge() {
		}

		typedef GenericNode<GlobalBBCFGNode, GlobalBBCFGEdge>::GEdgeSetTy BBCFGEdgeSet;
};

typedef GenericNode<GlobalBBCFGNode, GlobalBBCFGEdge> GenericBBCFGNodeTy;
// GlobalBBCFGNode needs to inherit llvm::Type to use dominator algorithm
class GlobalBBCFGNode : public llvm::Value, public GenericBBCFGNodeTy {
public:
		typedef GlobalBBCFGEdge::BBCFGEdgeSet CFGEdgeSet;
		typedef GlobalBBCFGEdge::BBCFGEdgeSet::iterator iterator;
		typedef GlobalBBCFGEdge::BBCFGEdgeSet::const_iterator const_iterator;

private:
		const llvm::BasicBlock* bb;
		GlobalBBCFG* graph;

public:
		GlobalBBCFGNode(NodeID i, const llvm::BasicBlock* b, GlobalBBCFG* g) : GenericBBCFGNodeTy(i, 0), bb(b), graph(g), 
									Value(llvm::Type::getLabelTy(b->getContext()), i){
		}

		inline const llvm::BasicBlock* getBasicBlock() const {
				return bb;
		}

		// getParent is needed to use dominator algorithm
		GlobalBBCFG* getParent() {
				return graph;
		}
};

typedef GenericGraph<GlobalBBCFGNode, GlobalBBCFGEdge> GenericBBCFGTy;
class GlobalBBCFG : public GenericBBCFGTy {
public:
		typedef GlobalBBCFGEdge::BBCFGEdgeSet BBCFGEdgeSet;
		typedef llvm::DenseMap<const llvm::BasicBlock*, GlobalBBCFGNode*> BasicBlockToCFGNodeMap;



private:
		llvm::Module* mod;
		PTACallGraph* ptaCallGraph; // pta call graph to build CallRetEdge between basic blocks
		NodeID controlFlowGraphNodeNum;
		BasicBlockToCFGNodeMap basicBlockToCFGNodeMap;
/*	
		std::vector<SccNode> SccInCFG; 	
		SccMap NodeSccMap;
*/

		void buildGlobalBBCFG(llvm::Module* module);

		void addAllBBCFGNode(llvm::Module* module);

		void addAllNaiveBBCFGEdge(llvm::Module* module);

		void addAllAdvancedBBCFGEdge(llvm::Module* module);

		void addBBCFGNode(const llvm::BasicBlock* bb);

		void addBBCFGEdge(const llvm::BasicBlock* src, const llvm::BasicBlock* dst, GlobalBBCFGEdge::CEDGEK kind);

		void destroy();

public:
		GlobalBBCFG(llvm::Module* module, PTACallGraph* callGraph): mod(module), 
								controlFlowGraphNodeNum(0), ptaCallGraph(callGraph) {
				buildGlobalBBCFG(module);
		}

		virtual ~GlobalBBCFG() {
				destroy();
		}

public:
		inline GlobalBBCFGNode* getCFGNode(const llvm::BasicBlock* bb) const {
				BasicBlockToCFGNodeMap::const_iterator it = basicBlockToCFGNodeMap.find(bb);
				assert(it!=basicBlockToCFGNodeMap.end() && "basic block node not found!!");
				return it->second;
		}

		bool hasGraphEdge(GlobalBBCFGNode* src, GlobalBBCFGNode* dst, GlobalBBCFGEdge::CEDGEK kind) const;

		inline void addEdge(GlobalBBCFGEdge* edge) {
				edge->getDstNode()->addIncomingEdge(edge);
				edge->getSrcNode()->addOutgoingEdge(edge);
		}

		inline llvm::Module* getModule() {
				return mod;
		}

		void dump(const std::string& filename);

		unsigned getCFGNodeNum() { return controlFlowGraphNodeNum; }

		GlobalBBCFGNode* getEntryBBNode() {
				return getCFGNode(&*mod->getFunction("main")->begin()); // assume all program begin from entry basic block of main function
		}

		GlobalBBCFGNode &front() { return *this->getEntryBBNode(); } 

		std::map<GlobalBBCFGNode*, unsigned> computeBBScore(unsigned (*scorefunc)(GlobalBBCFGNode*, std::set<int64_t> &),
                                                        std::set<int64_t> &filter);

		std::vector<const llvm::BasicBlock* > getOutGoingEdges(const llvm::BasicBlock* src);

		bool terminatingInst(llvm::Instruction &inst);

};

namespace llvm {
template<> struct GraphTraits<GlobalBBCFGNode*> : public GraphTraits<GenericNode<GlobalBBCFGNode, GlobalBBCFGEdge>* > {
		typedef GlobalBBCFGNode *NodeRef; // NodeRef is a member will be used in GraphTraits
};

template<>
struct GraphTraits<Inverse<GlobalBBCFGNode *>> : public GraphTraits<Inverse<GenericNode<GlobalBBCFGNode, GlobalBBCFGEdge>*>> {
		typedef GlobalBBCFGNode *NodeRef;
};

template<> struct GraphTraits<GlobalBBCFG *> : public GraphTraits<GenericGraph<GlobalBBCFGNode, GlobalBBCFGEdge>* > {
		typedef GlobalBBCFGNode *NodeRef;
		// getEntryNode is neede for dominator algorithm
		static NodeRef getEntryNode(GlobalBBCFG* G) { return G->getEntryBBNode() ;}
		static unsigned size(GlobalBBCFG* G) { return G->getCFGNodeNum(); }
};
}

#endif
