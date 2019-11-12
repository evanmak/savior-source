#include "DMA/GlobalBBDomTree.h"
#include "Util/GraphUtil.h"
#include <llvm/IR/Dominators.h>
#include <llvm/Support/GenericDomTreeConstruction.h>
#include <llvm/Support/DOTGraphTraits.h>
using namespace llvm;

template class DomTreeNodeBase<GlobalBBCFGNode>;
template class DominatorTreeBase<GlobalBBCFGNode>;

template void llvm::Calculate<GlobalBBCFG, GlobalBBCFGNode *>(
				DominatorTreeBase<
					typename std::remove_pointer<GraphTraits<GlobalBBCFGNode *>::NodeRef>::type>
					&DT,
					GlobalBBCFG &G);

template void llvm::Calculate<GlobalBBCFG, Inverse<GlobalBBCFGNode *>>(
				DominatorTreeBase<
					typename std::remove_pointer<GraphTraits<Inverse<GlobalBBCFGNode *>>::NodeRef>::type> 
					&DT, 
					GlobalBBCFG &G);


void GlobalBBDomTree::dump(const std::string& filename) {
		GraphPrinter::WriteGraphToFile(llvm::outs(), filename, this);
}

namespace llvm {
template<>
struct DOTGraphTraits<GlobalBBDomTree*> : public DefaultDOTGraphTraits {
		typedef GlobalBBDomTreeNode NodeType;
		typedef NodeType::iterator ChildIteratorType;
		DOTGraphTraits(bool isSimple = false) :
				DefaultDOTGraphTraits(isSimple){
				}
		static std::string getGraphName(GlobalBBDomTree* tree) {
				return "Global Basic Block Dominator Tree";
		}

		static std::string getNodeLabel(GlobalBBDomTreeNode* node, GlobalBBDomTree* tree) {
				return node->getBlock()->getBasicBlock()->getParent()->getName().str() + "\n" + node->getBlock()->getBasicBlock()->getName().str();
		}

		static std::string getNodeAttributes(GlobalBBDomTreeNode* node, GlobalBBDomTree* tree) {
				return "shape=circle";
		}

		template<class EdgeIter>
		static std::string getEdgeAttributes(GlobalBBDomTreeNode* node, EdgeIter EI, GlobalBBDomTree* globalBBDomTree) {
				std::string color;
				color = "color=red";
				return color;
		}
};
}
