#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/PostDominators.h"
#include <unordered_set>

using namespace llvm;

namespace {
struct kdcfgl : public FunctionPass {
	static char ID;
	kdcfgl() : FunctionPass(ID) {}


	void getAnalysisUsage(AnalysisUsage &AU) const{
		AU.addRequired<BlockFrequencyInfoWrapperPass>();
		AU.addRequired<BranchProbabilityInfoWrapperPass>();
		AU.addRequired<PostDominatorTreeWrapperPass>();
	}
	

	bool runOnFunction(Function &F) override {
		BranchProbabilityInfo &BPI = getAnalysis<BranchProbabilityInfoWrapperPass>().getBPI();
		BlockFrequencyInfo &BFI = getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();	
		
		//
		
		return true;
	}

	void getKeyDependentBranchedBlocks(BasicBlock* start_block, std::vector<BasicBlock*>& sorted_blocks) {
		PostDominatorTree &PDT = getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
		std::unordered_set<BasicBlock*> visited;
		getKeyDependentBranchedBlocksByTopologicalSort(start_block, start_block, PDT, visited, sorted_blocks);
	}

	void getKeyDependentBranchedBlocksByTopologicalSort(const BasicBlock* start_block, BasicBlock* block, const PostDominatorTree &PDT,
		std::unordered_set<BasicBlock*>& visited, std::vector<BasicBlock*>& sorted_blocks) {
		visited.insert(block);
		if (start_block != block) {
			bool immediate_post_dominator = PDT.dominates(block, start_block);
			if (immediate_post_dominator) {
				sorted_blocks.push_back(block);
				return;
			}
		}
		for (BasicBlock* suc : successors(block)) {
			auto it = visited.find(suc);
			if (it == visited.end()) {
				getKeyDependentBranchedBlocksByTopologicalSort(start_block, suc, PDT, visited, sorted_blocks);
			}
		}
		sorted_blocks.push_back(block);
	}
}; // end of struct Hello
}  // end of anonymous namespace

char kdcfgl::ID = 0;
static RegisterPass<kdcfgl> X("kdcfgl", "Key-Dependent Control Flow Linearization Pass");

