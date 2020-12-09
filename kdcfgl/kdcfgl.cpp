#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Support/Format.h"
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

		std::vector<BasicBlock*> critical_branch;

		while (!key_dependent.empty()){
			Value *cur_v=key_dependent.front();
		        //errs() << cur_v->getName() << ".\n";
			key_dependent.pop_front();
			if (Instruction *cur_i = dyn_cast<Instruction>(cur_v)){
				if (isBranch(cur_i->getOpcode())){
					errs() << "found critical branch in BB " << cur_i->getParent()->getName() << "\n";
					critical_branch.push_back(cur_i->getParent());
				}	
			}
			for (User *u : cur_v->users()){
				key_dependent.push_back(dyn_cast<Value>(u));
			}
		}
		
		return true;
	}
}; // end of struct Hello
}  // end of anonymous namespace

char kdcfgl::ID = 0;
static RegisterPass<kdcfgl> X("kdcfgl", "Key-Dependent Control Flow Linearization Pass");

