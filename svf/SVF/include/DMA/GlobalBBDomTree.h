#ifndef __GLOBALDOMINATORTREE_H__
#define __GLOBALDOMINATORTREE_H__

#include "DMA/GlobalBBCFG.h"
#include <llvm/Support/GenericDomTree.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/SmallVector.h>

namespace llvm {

// use extern template to force the compiler to not instantiate a template 
// when you know that it will be instantiate somewhere else - implicit instantiation
extern template class DomTreeNodeBase<GlobalBBCFGNode>;
extern template class DominatorTreeBase<GlobalBBCFGNode>; 
extern template void Calculate<GlobalBBCFG, GlobalBBCFGNode *>(
				DominatorTreeBaseByGraphTraits<GraphTraits<GlobalBBCFGNode *>> &DT, GlobalBBCFG &G);

extern template void Calculate<GlobalBBCFG, Inverse<GlobalBBCFGNode *>>(
				DominatorTreeBaseByGraphTraits<GraphTraits<Inverse<GlobalBBCFGNode *>>> &DT,
				GlobalBBCFG &G);

typedef DomTreeNodeBase<GlobalBBCFGNode> GlobalBBDomTreeNode;

class GlobalBBDomTree : public DominatorTreeBase<GlobalBBCFGNode> {
private:
		GlobalBBCFG* cfg;

public:
		typedef DominatorTreeBase<GlobalBBCFGNode> Base;

		GlobalBBDomTree() : DominatorTreeBase<GlobalBBCFGNode>(false) {}
		explicit GlobalBBDomTree(GlobalBBCFG  &G) : DominatorTreeBase<GlobalBBCFGNode>(false), cfg(&G) {
				recalculate(G);
		}

		GlobalBBDomTreeNode* getImmediateDomNode(GlobalBBDomTreeNode* Node) {
				return Node->getIDom();
		}

		const BasicBlock* getImmediateDomBB(BasicBlock* bb) {
				// 1. cfg->getCFGNode(bb) to obtain GlobalBBCFGNode for Basic Block bb
				// 2. getNode(1) to obtain GlobalBBDomTreeNode for GlobalBBCFGNode 1
				// 3. 2->getIDom() to obtain dominator GlobalBBDomTreeNode for GlobalBBDomTreeNode 2
				// 4. 3->getBlock() to obtain GlobalBBCFGNode for GlobalBBDomTreeNode 3
				// 5. 4->getBasicBlock() to obtain Basic Block bb for GlobalBBCFGNode 4
				return getNode(cfg->getCFGNode(bb))->getIDom()->getBlock()->getBasicBlock();
		}

		// get all basic blocks dominated by Basic Block bb, including bb self
		void getDescendants(const BasicBlock* bb, SmallVectorImpl<const BasicBlock*> &result) const {
				result.clear();
				SmallVector<GlobalBBCFGNode*, 8> cfgNodeResult;
				// 1. cfg->getCFGNode(bb) to obtain GlobalBBCFGNode for Basic Block bb
				DominatorTreeBase<GlobalBBCFGNode>::getDescendants(cfg->getCFGNode(bb), cfgNodeResult);
				for (auto node : cfgNodeResult)
						result.push_back(node->getBasicBlock());
		}
		
		void dump(const std::string& filename);
};

template <class Node, class ChildIterator> struct GlobalBBDomTreeGraphTraitsBase {
		typedef Node *NodeRef;
		typedef ChildIterator ChildIteratorType;
		typedef df_iterator<Node *, df_iterator_default_set<Node*>> nodes_iterator;

		static NodeRef getEntryNode(NodeRef N) {return N;}
		static ChildIteratorType child_begin(NodeRef N) {return N->begin();}
		static ChildIteratorType child_end(NodeRef N) {return N->end();}

		static nodes_iterator nodes_begin(NodeRef N) {
				return df_begin(getEntryNode(N));
		}

		static nodes_iterator nodes_end(NodeRef N) { return df_end(getEntryNode(N));}
};

template<>
struct GraphTraits<GlobalBBDomTreeNode *> 
: public GlobalBBDomTreeGraphTraitsBase<GlobalBBDomTreeNode, GlobalBBDomTreeNode::iterator> {};

template<>
struct GraphTraits<const GlobalBBDomTreeNode *> 
: public DomTreeGraphTraitsBase<const GlobalBBDomTreeNode, GlobalBBDomTreeNode::const_iterator> {};

template<> struct GraphTraits<GlobalBBDomTree*>
: public GraphTraits<GlobalBBDomTreeNode*> {
		static NodeRef getEntryNode(GlobalBBDomTree* DT) { return DT->getRootNode(); }

		static nodes_iterator nodes_begin(GlobalBBDomTree* DT) {
				return df_begin(getEntryNode(DT));
		}
		static nodes_iterator nodes_end(GlobalBBDomTree* DT) {
				return df_end(getEntryNode(DT));
		}
};
} // End llvm namespace
#endif
