#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/PostDominators.h"
#include <vector>
#include <unordered_set>
#include <deque>


using namespace llvm;

bool isBranch(unsigned opCode){
	return opCode==Instruction::Br || opCode==Instruction::Switch || 
		opCode==Instruction::IndirectBr;
}

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
		
		std::deque<Value*> key_dependent;

		for (Function::iterator bb=F.begin(), e=F.end(); bb!=e; ++bb){
			for (BasicBlock::iterator i=bb->begin(), ie=bb->end(); i!=ie; ++i){
				//Instruction
				if (CallInst *ci=dyn_cast<CallInst>(i)){
					Function *fn = ci->getCalledFunction();
					//errs() << fn->getName() << ", ";
					if (fn->getName()=="llvm.var.annotation"){
						Value* k = ci->getArgOperand(0);
						Instruction *ki = dyn_cast<Instruction>(k);
						//errs() << ki->getOperand(0)->getName() << "\n";
						key_dependent.push_back(ki->getOperand(0));
						BasicBlock* curbb = ki->getParent();
						errs() << "key is in BB " << curbb->getName() << "\n";
					}
				}
			}
		}

		std::unordered_set<BasicBlock*> critical_branch;

		while (!key_dependent.empty()){
			Value *cur_v=key_dependent.front();
		        //errs() << cur_v->getName() << ".\n";
			key_dependent.pop_front();
			if (Instruction *cur_i = dyn_cast<Instruction>(cur_v)){
				if (isBranch(cur_i->getOpcode())){
					errs() << "found critical branch in BB " << cur_i->getParent()->getName() << "\n";
					critical_branch.insert(cur_i->getParent());
				}
			}
			for (User *u : cur_v->users()){
				key_dependent.push_back(dyn_cast<Value>(u));
			}
		}
		std::vector<BasicBlock*> sorted_blocks;
		getKeyDependentBranchedBlocks(critical_branch, sorted_blocks);
		print_related_blocks_in_topological_order(sorted_blocks);
		return true;
	}

	void print_related_blocks_in_topological_order(const std::vector<BasicBlock*>& sorted_blocks) {
		errs() << "print related blocks in topological_order:\n";
		for (auto bb : sorted_blocks) {
			errs() <<  bb->getName() << "\n";
		}
	}

	void getKeyDependentBranchedBlocks(std::unordered_set<BasicBlock*> start_blocks, std::vector<BasicBlock*>& sorted_blocks) {
		PostDominatorTree &PDT = getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
		std::unordered_set<BasicBlock*> visited;
		for (auto it_start_block = start_blocks.begin(); it_start_block != start_blocks.end(); it_start_block++) {
			auto it_visited = visited.find(*it_start_block);
			if (it_visited == visited.end()) {
				getKeyDependentBranchedBlocksByTopologicalSort(start_blocks, *it_start_block, *start_blocks.begin(), PDT, visited, sorted_blocks);
			}
		}
		std::reverse(sorted_blocks.begin(), sorted_blocks.end());
	}

	void getKeyDependentBranchedBlocksByTopologicalSort(const std::unordered_set<BasicBlock*> start_blocks, const BasicBlock* start_block, BasicBlock* block, const PostDominatorTree &PDT,
														std::unordered_set<BasicBlock*>& visited, std::vector<BasicBlock*>& sorted_blocks) {
		visited.insert(block);
		if (start_block != block) {
			bool immediate_post_dominator = PDT.dominates(block, start_block);
			if (immediate_post_dominator) {
				auto it = start_blocks.find(block);
				if (it == start_blocks.end()) {
					sorted_blocks.push_back(block);
					return;
				}
			}
		}
		for (BasicBlock* suc : successors(block)) {
			auto it_visited = visited.find(suc);
			if (it_visited == visited.end()) {
				getKeyDependentBranchedBlocksByTopologicalSort(start_blocks, start_block, suc, PDT, visited, sorted_blocks);
			}
		}
		sorted_blocks.push_back(block);
	}
}; // end of struct Hello
}  // end of anonymous namespace

char kdcfgl::ID = 0;
static RegisterPass<kdcfgl> X("kdcfgl", "Key-Dependent Control Flow Linearization Pass");

