#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <deque>


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
		AU.addRequired<PostDominatorTreeWrapperPass>();
	}
	

	bool runOnFunction(Function &F) override {
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
		print_masks();
		replaceWithUnconditionalBranch(sorted_blocks);
		replacePhiWithSelect(sorted_blocks);
		return true;
	}

	void replaceWithUnconditionalBranch(std::vector<BasicBlock*>& sorted_blocks) {
		if (sorted_blocks.size() < 2) {
			return;
		}
		for (size_t i = 0; i < sorted_blocks.size() - 1; i++) {
			auto it_I = sorted_blocks[i]->begin();
			while (it_I != sorted_blocks[i]->end()) {
				unsigned int opCode = it_I->getOpcode();
				if (opCode == Instruction::Br) {
					BranchInst* brI = dyn_cast<BranchInst>(it_I);
					if (brI->isConditional() || brI->getSuccessor(0) != sorted_blocks[i + 1]) {
						BranchInst* unconditional = BranchInst::Create(sorted_blocks[i + 1]);
						ReplaceInstWithInst(brI, unconditional);
						errs() << "jmp: " << unconditional->getParent()->getName() << " => " << unconditional->getSuccessor(0)->getName() << "\n";
					}
					break;
				}
				it_I++;
			}
		}
	}

	void replacePhiWithSelect(const std::vector<BasicBlock*>& sorted_blocks) {
		for (size_t i = 0; i < sorted_blocks.size(); i++) {
			auto it_I = sorted_blocks[i]->begin();
			while (it_I != sorted_blocks[i]->end()) {
				unsigned int opCode = it_I->getOpcode();
				if (opCode == Instruction::PHI) {
					PHINode* phi = dyn_cast<PHINode>(it_I);
					std::vector<SelectInst*> inserted;
					bool valid = true;
					unsigned int j = 1;
					BasicBlock* to = sorted_blocks[i];
					BasicBlock* from = phi->getIncomingBlock(j);
					auto edge = std::make_pair(from, to);
					auto it_edge = masks.find(edge);
					SelectInst* select;
					if (it_edge != masks.end() && it_edge->second != nullptr) {
						Twine select_name = Twine("select_").concat(it_edge->second->getName());
						select = SelectInst::Create(it_edge->second, phi->getIncomingValue(j), phi->getIncomingValue(j - 1), select_name, phi);
						inserted.push_back(select);
						errs() << "insert select: " << select->getName() << "\n";
					} else {
						valid = false;
					}
					if (valid) {
						for (j = 2; j < phi->getNumIncomingValues(); j++) {
							from = phi->getIncomingBlock(j);
							edge = std::make_pair(from, to);
							it_edge = masks.find(edge);
							if (it_edge != masks.end() && it_edge->second != nullptr) {
								Twine select_name = Twine("select_").concat(it_edge->second->getName());
								select = SelectInst::Create(it_edge->second, phi->getIncomingValue(j), select, select_name, phi);
								inserted.push_back(select);
							} else {
								valid = false;
								break;
							}
						}
					}
					if (!valid) {
						for (auto rit_si = inserted.rbegin(); rit_si != inserted.rend(); rit_si++) {
							(*rit_si)->eraseFromParent();
						}
						it_I++;
					} else {
						auto it_rm = it_I;
						errs() << "remove phi: " << it_rm->getName() << "; its users are"<< "\n";
						for (auto user : it_rm->users()) {
							errs() << user->getName() << "\n";
						}
						it_rm->replaceAllUsesWith(select);
						it_I++;
						it_rm->eraseFromParent();
					}
				} else {
					it_I++;
				}
			}
		}
	}

	void print_related_blocks_in_topological_order(const std::vector<BasicBlock*>& sorted_blocks) const {
		errs() << "print related blocks in topological order:\n";
		for (auto bb : sorted_blocks) {
			errs() <<  bb->getName() << "\n";
		}
	}

	void print_masks() const {
		errs() << "print masks:\n";
		for (auto pair : masks) {
			errs() << "(" << (pair.first.first != nullptr ? pair.first.first->getName() : "null") << ", " << pair.first.second->getName() << "): "
			 << (pair.second != nullptr ? pair.second->getName() : "null") << ", " << (uintptr_t)pair.second << "\n";
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
			std::pair<BasicBlock*, BasicBlock*> edge = std::make_pair(nullptr, *it_start_block);
			auto it_visited = masks.find(edge);
			if (it_visited == masks.end()) {
				//masks.insert(std::make_pair(edge, nullptr));
				createMasksForKeyDependentBranchedBlocksDFS(start_blocks, *it_start_block, nullptr, *start_blocks.begin(), PDT);
			}
		}
	}

	void createMasksForKeyDependentBranchedBlocksDFS(const std::unordered_set<BasicBlock*> start_blocks, const BasicBlock* start_block, BasicBlock* from, BasicBlock* block, const PostDominatorTree &PDT) {
		if (start_block != block) {
			bool immediate_post_dominator = PDT.dominates(block, start_block);
			if (immediate_post_dominator) {
				auto it = start_blocks.find(block);
				if (it == start_blocks.end()) {
					return;
				}
			}
		}
		std::pair<BasicBlock*, BasicBlock*> start_edge = std::make_pair(nullptr, block);
		auto it_start_edge = masks.find(start_edge);
		std::pair<BasicBlock*, BasicBlock*> entry_edge = std::make_pair(from, block);
		if (from == nullptr) {	// directly called by createMasksForKeyDependentBranchedBlocks
			masks.insert(std::make_pair(start_edge, nullptr));
		} else {	// recursively called by createMasksForKeyDependentBranchedBlocksDFS
			auto it_entry_edge = masks.find(entry_edge);
			if (it_start_edge == masks.end() || it_start_edge->second == nullptr) {	// no exising start mask
				masks[start_edge] = it_entry_edge->second;
			} else {	// update exising start mask
				if (it_start_edge->second == it_entry_edge->second) {	// nothing to update
					return;
				}
				Instruction* exisiting_start_mask = dyn_cast<Instruction>(it_start_edge->second);
				Twine mask_name = Twine("mask_").concat(block->getName());
				if (exisiting_start_mask->getParent() != block) {	// exising start mask is NOT in the block
					Instruction* first_instruction = &(*block->begin());
					while (first_instruction->getOpcode() == Instruction::PHI) {
						first_instruction = first_instruction->getNextNode();
					}
					it_start_edge->second = BinaryOperator::Create(BinaryOperator::Or, exisiting_start_mask, it_entry_edge->second, mask_name, first_instruction);
				} else {	// exising start mask is in the block
					it_start_edge->second = BinaryOperator::Create(BinaryOperator::Or, exisiting_start_mask, it_entry_edge->second, mask_name, exisiting_start_mask->getNextNode());
				}
			}
		}
		it_start_edge = masks.find(start_edge);
		for (BasicBlock::iterator I = block->begin(), E = block->end(); I != E; ++I) {
			unsigned int opCode = I->getOpcode();
			if (opCode == Instruction::Br) {
				BranchInst* brI = dyn_cast<BranchInst>(I);
				if (brI->isConditional()) {
					Value* predicate = brI->getCondition();
					BasicBlock* suc = brI->getSuccessor(0);
					Twine mask_true_name = Twine("mask_").concat(block->getName()).concat("_").concat(suc->getName());
					std::pair<BasicBlock*, BasicBlock*> edge = std::make_pair(block, suc);
					auto it_edge = masks.find(edge);
					if (it_edge == masks.end()) {
						if (it_start_edge == masks.end() || it_start_edge->second == nullptr) {
							masks.insert(make_pair(edge, predicate));
						} else {
							BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_start_edge->second, predicate, mask_true_name, brI);
							//errs() << mask_edge->getName() << ", " << (uintptr_t)mask_edge << "\n";
							masks.insert(make_pair(edge, mask_edge));
						}
					} else {
						BinaryOperator* exisited_mask_edge = dyn_cast<BinaryOperator>(it_edge->second);
						BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_start_edge->second, predicate, mask_true_name, exisited_mask_edge);
						exisited_mask_edge->replaceAllUsesWith(mask_edge);
						exisited_mask_edge->eraseFromParent();
						//errs() << mask_edge->getName() << ", " << (uintptr_t)mask_edge << "\n";
						it_edge->second = mask_edge;
					}
					createMasksForKeyDependentBranchedBlocksDFS(start_blocks, start_block, block, suc, PDT);
					suc = brI->getSuccessor(1);
					edge = std::make_pair(block, suc);
					it_edge = masks.find(edge);
					Twine mask_not_name = Twine("mask_not_").concat(predicate->getName());
					Twine mask_false_name = Twine("mask_").concat(block->getName()).concat("_").concat(suc->getName());
					BinaryOperator* not_predicate = BinaryOperator::CreateNot(predicate, mask_not_name, brI);
					if (it_edge == masks.end()) {
						if (it_start_edge == masks.end() || it_start_edge->second == nullptr) {
							masks.insert(make_pair(edge, not_predicate));
						} else {
							BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_start_edge->second, not_predicate, mask_false_name, brI);
							//errs() << mask_edge->getName() << ", " << (uintptr_t)mask_edge << "\n";
							masks.insert(make_pair(edge, mask_edge));
						}
					} else {
						BinaryOperator* exisited_mask_edge = dyn_cast<BinaryOperator>(it_edge->second);
						BinaryOperator* mask_edge = BinaryOperator::Create(BinaryOperator::And, it_start_edge->second, not_predicate, mask_false_name, exisited_mask_edge);
						exisited_mask_edge->replaceAllUsesWith(mask_edge);
						exisited_mask_edge->eraseFromParent();
						//errs() << mask_edge->getName() << ", " << (uintptr_t)mask_edge << "\n";
						it_edge->second = mask_edge;
					}
					createMasksForKeyDependentBranchedBlocksDFS(start_blocks, start_block, block, suc, PDT);
				} else {
					BasicBlock* suc = brI->getSuccessor(0);
					std::pair<BasicBlock*, BasicBlock*> edge = std::make_pair(block, suc);
					masks[edge] = it_start_edge->second;
					createMasksForKeyDependentBranchedBlocksDFS(start_blocks, start_block, block, suc, PDT);
				}
			}
		}
	}
}; // end of struct kdcfgl
}  // end of anonymous namespace

char kdcfgl::ID = 0;
static RegisterPass<kdcfgl> X("kdcfgl", "Key-Dependent Control Flow Linearization Pass");

