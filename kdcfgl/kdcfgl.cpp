#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/InstrTypes.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <utility>


using namespace llvm;

bool isBranch(unsigned opCode){
	return opCode==Instruction::Br || opCode==Instruction::Switch || 
		opCode==Instruction::IndirectBr;
}

struct hash_pair { 
    template <class T1, class T2> 
    size_t operator()(const std::pair<T1, T2>& p) const { 
        auto hash1 = std::hash<T1>()(p.first); 
        auto hash2 = std::hash<T2>()(p.second); 
        return (hash1*17) ^ hash2; 
    } 
};

namespace {
struct kdcfgl : public FunctionPass {
	std::unordered_map<std::pair<BasicBlock*, BasicBlock*>, Value*, hash_pair> masks;
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
		createMasksForKeyDependentBranchedBlocks(critical_branch);
		return true;
	}

	void print_related_blocks_in_topological_order(const std::vector<BasicBlock*>& sorted_blocks) {
		errs() << "print related blocks in topological order:\n";
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

	void createMasksForKeyDependentBranchedBlocks(std::unordered_set<BasicBlock*> start_blocks) {
		PostDominatorTree &PDT = getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
		for (auto it_start_block = start_blocks.begin(); it_start_block != start_blocks.end(); it_start_block++) {
			std::pair<BasicBlock*, BasicBlock*> edge = std::make_pair(*it_start_block, *it_start_block);
			auto it_visited = masks.find(edge);
			if (it_visited == masks.end()) {
				masks.insert(std::make_pair(edge, nullptr));
				createMasksForKeyDependentBranchedBlocksDFS(start_blocks, *it_start_block, *start_blocks.begin(), PDT);
			}
		}
	}

	void createMasksForKeyDependentBranchedBlocksDFS(const std::unordered_set<BasicBlock*> start_blocks, const BasicBlock* start_block, BasicBlock* block, const PostDominatorTree &PDT) {
		if (start_block != block) {
			bool immediate_post_dominator = PDT.dominates(block, start_block);
			if (immediate_post_dominator) {
				auto it = start_blocks.find(block);
				if (it == start_blocks.end()) {
					return;
				}
			}
		}
		for (BasicBlock::iterator I = block->begin(), E = block->end(); I != E; ++I) {
			unsigned int opCode = I->getOpcode();
			if (opCode == Instruction::Br) {
				BranchInst* brI = dyn_cast<BranchInst>(I);
				if (brI->isConditional()) {
					Value* predicate = brI->getOperand(0);
					BasicBlock* suc = brI->getSuccessor(0);
					std::pair<BasicBlock*, BasicBlock*> edge = std::make_pair(block, suc);
					auto it_edge = masks.find(edge);
					if (it_edge == masks.end()) {
						std::pair<BasicBlock*, BasicBlock*> entry = std::make_pair(block, block);
						auto it_entry = masks.find(entry);
						std::pair<BasicBlock*, BasicBlock*> suc_entry = std::make_pair(suc, suc);
						auto it_suc_entry = masks.find(suc_entry);
						if (it_entry == masks.end() || it_entry->second == nullptr) {
							masks.insert(make_pair(edge, predicate));
							if (it_suc_entry == masks.end() || it_suc_entry->second == nullptr) {
								masks[suc_entry] = predicate;
							} else {
								BinaryOperator* suc_entry_edge;
								if (dyn_cast<Instruction>(it_suc_entry->second)->getParent() == suc) {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, predicate, "", dyn_cast<Instruction>(it_suc_entry->second)->getNextNode());
								} else {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, predicate, "", &(suc->front()));
								}
								masks[suc_entry] = suc_entry_edge;
							}
						} else {
							BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_entry->second, predicate, "", brI);
							masks.insert(make_pair(edge, mask_edge));
							if (it_suc_entry == masks.end() || it_suc_entry->second == nullptr) {
								masks[suc_entry] = mask_edge;
							} else {
								BinaryOperator* suc_entry_edge;
								if (dyn_cast<Instruction>(it_suc_entry->second)->getParent() == suc) {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, mask_edge, "", dyn_cast<Instruction>(it_suc_entry->second)->getNextNode());
								} else {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, mask_edge, "", &(suc->front()));
								}
								masks[suc_entry] = suc_entry_edge;
							}
						}
						createMasksForKeyDependentBranchedBlocksDFS(start_blocks, start_block, suc, PDT);
					}
					suc = brI->getSuccessor(1);
					edge = std::make_pair(block, suc);
					it_edge = masks.find(edge);
					if (it_edge == masks.end()) {
						BinaryOperator* not_predicate = BinaryOperator::CreateNot(predicate, "", brI);
						std::pair<BasicBlock*, BasicBlock*> entry = std::make_pair(block, block);
						auto it_entry = masks.find(entry);
						std::pair<BasicBlock*, BasicBlock*> suc_entry = std::make_pair(suc, suc);
						auto it_suc_entry = masks.find(suc_entry);
						if (it_entry == masks.end() || it_entry->second == nullptr) {
							masks.insert(make_pair(edge, not_predicate));
							if (it_suc_entry == masks.end() || it_suc_entry->second == nullptr) {
								masks[suc_entry] = predicate;
							} else {
								BinaryOperator* suc_entry_edge;
								if (dyn_cast<Instruction>(it_suc_entry->second)->getParent() == suc) {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, not_predicate, "", dyn_cast<Instruction>(it_suc_entry->second)->getNextNode());
								} else {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, not_predicate, "", &(suc->front()));
								}
								masks[suc_entry] = suc_entry_edge;
							}
						} else {
							BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_entry->second, not_predicate, "", brI);
							masks.insert(make_pair(edge, mask_edge));
							if (it_suc_entry == masks.end() || it_suc_entry->second == nullptr) {
								masks[suc_entry] = mask_edge;
							} else {
								BinaryOperator* suc_entry_edge;
								if (dyn_cast<Instruction>(it_suc_entry->second)->getParent() == suc) {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, mask_edge, "", dyn_cast<Instruction>(it_suc_entry->second)->getNextNode());
								} else {
									suc_entry_edge = BinaryOperator::Create(BinaryOperator::Or, it_suc_entry->second, mask_edge, "", &(suc->front()));
								}
								masks[suc_entry] = suc_entry_edge;
							}
						}
						createMasksForKeyDependentBranchedBlocksDFS(start_blocks, start_block, suc, PDT);
					}
				}
			}
		}
	}
}; // end of struct Hello
}  // end of anonymous namespace

char kdcfgl::ID = 0;
static RegisterPass<kdcfgl> X("kdcfgl", "Key-Dependent Control Flow Linearization Pass");

