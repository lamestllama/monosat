/****************************************************************************************[Solver.h]
 The MIT License (MIT)

 Copyright (c) 2014, Sam Bayless

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **************************************************************************************************/

#ifndef BV_THEORY_SOLVER_H_
#define BV_THEORY_SOLVER_H_
#include <cstddef>
#include <gmpxx.h>
#include "utils/System.h"
#include "core/Theory.h"

#include "core/SolverTypes.h"
#include "mtl/Map.h"

#include "BVTheory.h"
#include "utils/System.h"
#include "core/TheorySolver.h"

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <iostream>
template<typename Weight>
class BVTheorySolver;

namespace Monosat {

enum class Comparison{
		lt,leq,gt,geq
};
inline Comparison operator ~(Comparison p) {
	switch (p){
		case Comparison::lt:
			return Comparison::gt;
		case Comparison::leq:
			return Comparison::geq;
		case Comparison::gt:
			return Comparison::lt;
		default:
			return Comparison::leq;
	}
}
inline Comparison operator -(Comparison p) {
	switch (p){
		case Comparison::lt:
			return Comparison::geq;
		case Comparison::leq:
			return Comparison::gt;
		case Comparison::gt:
			return Comparison::leq;
		default:
			return Comparison::lt;
	}
}
inline std::ostream& operator<<(std::ostream &out, const Comparison &p) {
	switch (p){
		case Comparison::lt:
			return out <<"<";
		case Comparison::leq:
			return out <<"<=";
		case Comparison::gt:
			return out <<">";
		default:
			return out <<">=";
	}
}

template<typename Weight>
class BVTheorySolver: public Theory {
public:

	struct Cause{
		int cause_is_bits:1;
		int refined_cause:1;
		int cause_is_addition:1;
		int comparison_cause:29;
		int cause_is_addition_arg:32;
		Cause(const Cause & copy):cause_is_bits(copy.cause_is_bits),refined_cause(copy.refined_cause),cause_is_addition(copy.cause_is_addition),comparison_cause(copy.comparison_cause),cause_is_addition_arg(copy.cause_is_addition_arg){

		}

		Cause():cause_is_bits(0),refined_cause(0),cause_is_addition(0),cause_is_addition_arg(-1),comparison_cause(-1){

		}
		/*Cause(bool bits, bool addition, int comparison=-1):cause_is_bits(bits),refined_cause(0),cause_is_addition(addition),cause_is_addition_arg(0),comparison_cause(comparison){

		}*/
		bool hasCause(){
			//this can be improved, if we want.
			return cause_is_bits|| cause_is_addition ||  (cause_is_addition_arg>=0)  || (comparison_cause>=0) || refined_cause;
		}
		void clear(){
			cause_is_bits=0;
			refined_cause=0;
			cause_is_addition=0;
			cause_is_addition_arg=-1;
			comparison_cause=-1;
		}
	};
	struct Assignment {
		bool isComparator :1;
		bool assign :1;
		int bvID:30;
		Var var;
		Weight previous_under;
		Weight previous_over;
		Weight new_under;
		Weight new_over;

		Cause prev_under_cause;
		Cause prev_over_cause;
		Cause new_under_cause;
		Cause new_over_cause;
		Assignment(bool isComparator, bool assign, int bvID, Var v,Weight previous_under,Weight previous_over,Weight new_under,Weight new_over,
				Cause prev_under_cause,Cause prev_over_cause,Cause new_under_cause,Cause new_over_cause):isComparator(isComparator),assign(assign),bvID(bvID),var(v),previous_under(previous_under),previous_over(previous_over),new_under(new_under),new_over(new_over),
				prev_under_cause(prev_under_cause),prev_over_cause(prev_over_cause),new_under_cause(new_under_cause),new_over_cause(new_over_cause){

		}
	};

	struct ComparisonID{
		Weight w;
		Lit l;
		int compareID;
		int bvID:30;
		int is_lt:1;
		int is_strict:1;

		ComparisonID(Weight w, int compareID, Lit l,int bvID, Comparison op ):w(w),l(l),compareID(compareID),bvID(bvID),is_lt(op==Comparison::lt || op==Comparison::leq),is_strict(op==Comparison::lt || op==Comparison::gt){

		}

		bool bvCompare(){
			return compareID>-1;
		}

		Comparison op(){
			if(is_lt && is_strict){
				return Comparison::lt;
			}else if(is_lt){
				return Comparison::leq;
			}else if(is_strict){
				return Comparison::gt;
			}else{
				return Comparison::geq;
			}
		}
	};


	class BitVector{
		BVTheorySolver * outer;
		int id;
	public:
		BitVector():outer(nullptr),id(0){

			}
		BitVector(BVTheorySolver & outer, int id):outer(&outer),id(id){

		}
		BitVector(const BitVector & copy):outer(copy.outer),id(copy.id){

			}
		BitVector(const BitVector && move):outer(move.outer),id(move.id){

			}
		BitVector& operator=(BitVector   other) {
			outer= other.outer;
			id=other.id;
			return *this;
		}

		int getID(){
			return id;
		}
		Weight & getUnder(bool level0=false){
			return outer->getUnderApprox(id,level0);
		}
		Weight & getOver(bool level0=false){
			return outer->getOverApprox(id,level0);
		}

		vec<Lit> & getBits()const{
			return outer->getBits(id);
		}
		int width()const{
			return getBits().size();
		}
	};

	//typedef void (*CallBack)(int) ;

	double rnd_seed;

private:
	TheorySolver * S;

	bool comp(Comparison op, Weight o1, Weight o2){
		switch (op){
			case Comparison::lt:
				return o1<o2;
			case Comparison::leq:
				return  o1<=o2;
			case Comparison::gt:
				return  o1>o2;
			default:
				return o1>=o2;
		}
	}

public:
	int id;
	bool first_propagation=true;
	long n_bits =0;
	long n_consts = 0;
	long n_starting_consts=0;
	vec<lbool> assigns;
	CRef comparisonprop_marker;
	CRef bvprop_marker;

	vec<Assignment> trail;
	vec<int> trail_lim;

	vec<Cause> under_causes;
	vec<Cause> over_causes;
	//Bitvectors are unsigned, and have least significant bit at index 0
	vec<vec<Lit> > bitvectors;
	vec<ComparisonID> comparisons;
	vec<vec<int> > cause_set;//for each bv, this is the set of all bitvectors that have a greater id and might need to have their approx updated when this bv's approx changes.
	vec<vec<int> > compares; //for each bitvector, comparisons are all to unique values, and in ascending order of compareTo.
	vec<vec<int>> bvcompares;

	struct Addition{
		int aID=-1;
		int bID=-1;

		bool hasAddition()const{
			return aID>=0;
		}

	};
	struct AdditionArg{
		int other_argID=-1;
		int sumID=-1;
	};

	vec<Addition> additions;//each bitvector is the result of at most one addition.
	vec<vec<AdditionArg>> addition_arguments;
	vec<Weight> under_approx0;//under approx at level 0
	vec<Weight> over_approx0;//over approx at level 0
	vec<Weight> under_approx;
	vec<Weight> over_approx;
	vec<int> theoryIds;
	vec<int> altered_bvs;
	vec<bool> alteredBV;

	vec<int> backtrack_altered;

	vec<bool> bvconst;

	int analysis_trail_pos = -1;

	vec<bool> in_backtrack_queue;

	vec<int> backtrack_queue;
	vec<BVTheory*> theories;
	vec<BVTheory*> actual_theories;
public:


	//vec<int> marker_map;

	bool requiresPropagation = true;

	vec<char> seen;
	vec<int> to_visit;
	vec<Lit> tmp_clause;
	//Data about local theory variables, and how they connect to the sat solver's variables
	struct VarData {

		int occursPositive :1;
		int occursNegative :1;

		int bvID:30;
		int comparisonID;
		Var solverVar;
	};

	vec<VarData> vars;
	int theory_index = 0;
public:

	double reachtime = 0;
	double unreachtime = 0;
	double pathtime = 0;
	double propagationtime = 0;
	double stats_conflict_time = 0;
	long stats_propagations = 0;
	long stats_num_conflicts = 0;
	long stats_bit_conflicts = 0;
	long stats_addition_conflicts = 0;
	long stats_compare_conflicts = 0;
	long stats_bv_compare_conflicts = 0;
	long stats_decisions = 0;
	long stats_num_reasons = 0;
	long stats_build_value_reason=0;
	long stats_build_value_bv_reason=0;
	long stats_build_addition_reason=0;
	long stats_build_addition_arg_reason =0;
	double reachupdatetime = 0;
	double unreachupdatetime = 0;
	double stats_initial_propagation_time = 0;
	double stats_decision_time = 0;
	double stats_reason_initial_time = 0;
	double stats_reason_time = 0;
	long num_learnt_paths = 0;
	long learnt_path_clause_length = 0;
	long num_learnt_cuts = 0;
	long learnt_cut_clause_length = 0;
	long stats_pure_skipped = 0;
	long stats_mc_calls = 0;
	long stats_propagations_skipped = 0;
	long statis_bv_updates = 0;

	BVTheorySolver(TheorySolver * S ) :
			S(S){
		rnd_seed = opt_random_seed;
		S->addTheory(this);
		comparisonprop_marker = S->newReasonMarker(this);
		bvprop_marker = S->newReasonMarker(this);

	}
	~BVTheorySolver(){

	}
	TheorySolver * getSolver(){
		return S;
	}

	bool hasTheory(int bvID){
		assert(bvID>=0);
		return theoryIds[bvID]>=0;
	}
	BVTheory * getTheory(int bvID){
		assert(hasTheory(bvID));
		return theories[theoryIds[bvID]];
	}

	void addTheory(BVTheory* theory){
		theories.growTo(theory->getTheoryIndexBV()+1);
		actual_theories.push(theory);
		theories[theory->getTheoryIndexBV()]=theory;
	}

	void printStats(int detailLevel) {
		printf("BV Theory %d stats:\n", this->id);

		printf("%d bitvectors, %ld bits, %d comparisons (bvcomparisons), %d additions\n", bitvectors.size(),n_bits,compares.size()+bvcompares.size(),bvcompares.size(), additions.size()				 );
		printf("constant bitvectors (at start, end of deduction): %d, %d\n",n_starting_consts ,n_consts);


		printf("Propagations: %ld (%f s, avg: %f s, %ld skipped), bv updates: %d\n", stats_propagations, propagationtime,
				(propagationtime) / ((double) stats_propagations + 1), stats_propagations_skipped,statis_bv_updates);
		printf("Decisions: %ld (%f s, avg: %f s)\n", stats_decisions, stats_decision_time,
				(stats_decision_time) / ((double) stats_decisions + 1));
		printf("Conflicts: %ld (bits: %ld, additions: %ld, comparisons: %ld, bv comparisons: %ld), %f seconds\n", stats_num_conflicts,stats_bit_conflicts,stats_addition_conflicts,stats_compare_conflicts,stats_bv_compare_conflicts, stats_conflict_time);
		printf("Reasons: %ld (%f s, avg: %f s)\n", stats_num_reasons, stats_reason_time,
				(stats_reason_time) / ((double) stats_num_reasons + 1));
		printf("Build: value reason %ld, bv value reason %ld, addition reason %ld\n", stats_build_value_reason,stats_build_value_bv_reason,stats_build_addition_reason);

		fflush(stdout);
	}
	
	void writeTheoryWitness(std::ostream& write_to) {
		for(int bvID = 0;bvID<bitvectors.size();bvID++){
				vec<Lit> & bv = bitvectors[bvID];
				updateApproximations(bvID);
				Weight under=under_approx[bvID];
				write_to<<"bv"<<bvID << " = " << under << "\n";

		}
	}
	
	inline int getTheoryIndex() {
		return theory_index;
	}
	inline void setTheoryIndex(int id) {
		theory_index = id;
	}


	BitVector getBV(int bvID){
		return BitVector(*this,bvID);
	}

	inline bool isComparisonVar(Var v) const{
		assert(v < vars.size());
		return vars[v].comparisonID>-1;
	}
	inline int getComparisonID(Var v) const {
		assert(isComparisonVar(v));
		return vars[v].comparisonID;
	}
	inline ComparisonID& getComparison(int comparisonID){
		return comparisons[comparisonID];
	}
	inline int getbvID(Var v) const{
		return vars[v].bvID;
	}

	void makeEqual(Lit l1, Lit l2) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		tmp_clause.clear();
		tmp_clause.push(~o1);
		tmp_clause.push(o2);
		S->addClauseSafely(tmp_clause);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(~o2);
		S->addClauseSafely(tmp_clause);
	}
	void makeEqualInSolver(Lit o1, Lit o2) {
		tmp_clause.clear();
		tmp_clause.push(~o1);
		tmp_clause.push(o2);
		S->addClauseSafely(tmp_clause);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(~o2);
		S->addClauseSafely(tmp_clause);
	}
	void addClause(Lit l1) {
		Lit o1 = toSolver(l1);
		tmp_clause.clear();
		tmp_clause.push(o1);
		S->addClauseSafely(tmp_clause);
	}
	void addClause(Lit l1, Lit l2) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(o2);

		S->addClauseSafely(tmp_clause);
	}
	void addClause(Lit l1, Lit l2, Lit l3) {
		Lit o1 = toSolver(l1);
		Lit o2 = toSolver(l2);
		Lit o3 = toSolver(l3);
		tmp_clause.clear();
		tmp_clause.push(o1);
		tmp_clause.push(o2);
		tmp_clause.push(o3);
		S->addClauseSafely(tmp_clause);
	}
	void addClause(vec<Lit> & c) {
		tmp_clause.clear();
		c.copyTo(tmp_clause);
		toSolver(tmp_clause);
		S->addClauseSafely(tmp_clause);
	}
	void addClauseSafely(vec<Lit> & c) {
		tmp_clause.clear();
		c.copyTo(tmp_clause);
		toSolver(tmp_clause);
		
		S->addClauseSafely(tmp_clause);
	}
	

	Var newVar(Var solverVar=var_Undef, int bvID=-1, int comparisonID=-1, bool connectToTheory = true) {
		if(solverVar==var_Undef){
			solverVar = S->newVar();
		}
		Var v = vars.size();
		if (connectToTheory) {
			S->newTheoryVar(solverVar, getTheoryIndex(), v);

		}else{
			while (S->nVars() <= solverVar)
				S->newVar();
		}


		vars.push();
		vars[v].occursPositive=false;
		vars[v].occursNegative=false;
		vars[v].bvID=bvID;
		vars[v].comparisonID=comparisonID;
		vars[v].solverVar = solverVar;

		assigns.push(l_Undef);
		if (connectToTheory) {
			assert(toSolver(v) == solverVar);
		}
		return v;
	}
	inline int level(Var v) {
		return S->level(toSolver(v));
	}
	inline int decisionLevel() {
		return trail_lim.size(); //S->decisionLevel();
	}
	inline int nVars() const {
		return vars.size(); //S->nVars();
	}
	inline Var toSolver(Var v) {
		//return v;
		assert(v < vars.size());
		//assert(S->hasTheory(vars[v].solverVar));
		//assert(S->getTheoryVar(vars[v].solverVar)==v);
		return vars[v].solverVar;
	}
	
	inline Lit toSolver(Lit l) {
		//assert(S->hasTheory(vars[var(l)].solverVar));
		//assert(S->getTheoryVar(vars[var(l)].solverVar)==var(l));
		return mkLit(vars[var(l)].solverVar, sign(l));
	}
	
	void toSolver(vec<Lit> & c) {
		for (int i = 0; i < c.size(); i++) {
			c[i] = toSolver(c[i]);
		}
	}
	
	inline lbool value(Var v) {
		if (assigns[v] != l_Undef)
			assert(S->value(toSolver(v)) == assigns[v]);
		
		return assigns[v]; //S->value(toSolver(v));
	}
	inline lbool value(Lit l) {
		if (assigns[var(l)] != l_Undef) {
			assert(S->value(toSolver(l)) == (assigns[var(l)] ^ sign(l)));
		}
		return assigns[var(l)] ^ sign(l);
	}
	inline lbool dbg_value(Var v) {
		return S->value(toSolver(v));
	}
	inline lbool dbg_value(Lit l) {
		return S->value(toSolver(l));
	}

	void enqueueEager(Lit l,int bvID, Weight prev_under, Weight prev_over, Cause prev_under_cause, Cause prev_over_cause, CRef reason_marker){
		enqueue(l,bvID,prev_under,prev_over,prev_under_cause,prev_over_cause,reason_marker);
		//if newly created lits are enqueued, then they must provide a reason eagerly (so that complex propagation dependencies can be handled correctly by the solver, instead of having to be sorted out in the theories).
		/*static int iter = 0;
		++iter;
		vec<Lit>  reason;
		buildReason(l,reason,reason_marker);

		Lit sl = toSolver(l);

		S->addClauseSafely(reason);

		//the lit must have been propagated by this clause (or, alternatively, the solver might now have a conflict).
		if(S->value(sl)==l_True)
			enqueueTheory(l);*/
	}

	inline bool enqueue(Lit l,int bvID, Weight prev_under, Weight prev_over, Cause prev_under_cause, Cause prev_over_cause, CRef reason) {
		assert(assigns[var(l)]==l_Undef);
		if(l.x==55){
			int a=1;
		}
		Lit sl = toSolver(l);
#ifndef NDEBUG

	printf("learnt ");
	for (int i = 0;i<nVars();i++){
			if(value(i)!=l_Undef){
				if(i!=var(l)){
					Lit l = mkLit(i,value(i)==l_True);
					assert(value(l)==l_False);
					printf(" %d",dimacs(toSolver(l)));
				}
			}
		}
	printf(" %d", dimacs(sl));
	printf(" 0\n");
#endif
		enqueueTheory(l,prev_under,prev_over,under_approx[bvID],over_approx[bvID],prev_under_cause,prev_over_cause, under_causes[bvID],over_causes[bvID]);
		if (S->enqueue(sl, reason)) {
			return true;
		} else {
			return false;
		}
	}

	void rewind_trail_pos(int trail_pos){
		if(analysis_trail_pos==trail_pos)
			return;
		if (analysis_trail_pos>=trail.size()){
			analysis_trail_pos=trail.size()-1;
		}
		if(trail.size()==0){
			analysis_trail_pos=0;
			return;
		}
		if(trail_pos<0){
			trail_pos=trail.size()-1;
		}
		if(trail_pos>=trail.size()){
			trail_pos=trail.size()-1;
		}
		if(analysis_trail_pos<0){
			analysis_trail_pos=0;
		}
		if(analysis_trail_pos<trail_pos){
			for(;analysis_trail_pos<trail_pos;analysis_trail_pos++){
				assert(analysis_trail_pos>=0);assert(analysis_trail_pos<trail.size()-1);
				Assignment & e = trail[analysis_trail_pos+1];
				int bvID;
				if (e.isComparator) {
					int cID = e.bvID;
					assert(cID>=0);
					ComparisonID & c = comparisons[cID];
					bvID = c.bvID;
				} else {
					bvID = e.bvID;

				}
				under_approx[bvID]=e.new_under;
				over_approx[bvID]=e.new_over;
				under_causes[bvID]=e.new_under_cause;
				over_causes[bvID]=e.new_over_cause;
			}

		}else if(analysis_trail_pos>trail_pos){
			for(;analysis_trail_pos>trail_pos;analysis_trail_pos--){
				assert(analysis_trail_pos>=0);assert(analysis_trail_pos<trail.size());
				Assignment & e = trail[analysis_trail_pos];
				int bvID;
				if (e.isComparator) {
					int cID = e.bvID;
					assert(cID>=0);
					ComparisonID & c = comparisons[cID];
					bvID = c.bvID;
				} else {
					bvID = e.bvID;
				}
				under_approx[bvID]=e.previous_under;
				over_approx[bvID]=e.previous_over;
				under_causes[bvID]=e.prev_under_cause;
				over_causes[bvID]=e.prev_over_cause;
			}
		}
		assert(analysis_trail_pos>=0);
		assert(analysis_trail_pos<=trail.size());
		assert(analysis_trail_pos==trail_pos);
	}

	int rewindUntil(int for_bvID, Comparison op, Weight compareTo){
		if(analysis_trail_pos<0){
			analysis_trail_pos=0;
		}
		if(analysis_trail_pos>=trail.size()){
			analysis_trail_pos=trail.size()-1;
		}
		//rewind until the previous under or over approx of bvID violates the comparison.
		for(;analysis_trail_pos>=0;analysis_trail_pos--){
			Assignment & e = trail[analysis_trail_pos];
			int bvID;
			if (e.isComparator) {
				int cID = e.bvID;
				assert(cID>=0);
				ComparisonID & c = comparisons[cID];
				bvID = c.bvID;
			} else {
				bvID = e.bvID;
			}

			if (bvID==for_bvID){
				if (op==Comparison::lt){
					assert(over_approx[for_bvID]<compareTo);
					if (e.previous_over >= compareTo){
						break;
					}
				}else if (op==Comparison::leq){
					assert(over_approx[for_bvID]<=compareTo);
					if (e.previous_over > compareTo){
						break;
					}
				}else if (op==Comparison::gt){
					assert(under_approx[for_bvID]>compareTo);
					if (e.previous_under <= compareTo){
						break;
					}
				}else if (op==Comparison::geq){
					assert(under_approx[for_bvID]>=compareTo);
					if (e.previous_under < compareTo){
						break;
					}
				}
			}

			under_approx[bvID]=e.previous_under;
			over_approx[bvID]=e.previous_over;
			under_causes[bvID]=e.prev_under_cause;
			over_causes[bvID]=e.prev_over_cause;
		}
		if(analysis_trail_pos<0){
			analysis_trail_pos=0;
		}
		assert(analysis_trail_pos>=0);
		assert(analysis_trail_pos<=trail.size());
		return analysis_trail_pos;
	}


	void backtrackUntil(int level) {
		static int it = 0;
		rewind_trail_pos(trail.size());
		//need to remove and add edges in the two graphs accordingly.
		if (trail_lim.size() > level) {
			
			int stop = trail_lim[level];
			for (int i = trail.size() - 1; i >= trail_lim[level]; i--) {
				
				Assignment & e = trail[i];
				assert(assigns[e.var]!=l_Undef);
				if (e.isComparator) {
					int cID = e.bvID;
					assert(cID>=0);
					ComparisonID & c = comparisons[cID];
					int bvID = c.bvID;
					under_approx[bvID]=e.previous_under;
					over_approx[bvID]=e.previous_over;
					under_causes[bvID]=e.prev_under_cause;
					over_causes[bvID]=e.prev_over_cause;
					if(!alteredBV[bvID]){
						alteredBV[bvID]=true;
						altered_bvs.push(bvID);
					}

				} else {

					int bvID = e.bvID;
					under_approx[bvID]=e.previous_under;
					over_approx[bvID]=e.previous_over;
					under_causes[bvID]=e.prev_under_cause;
					over_causes[bvID]=e.prev_over_cause;
					if(!alteredBV[bvID]){
						alteredBV[bvID]=true;
						altered_bvs.push(bvID);
					}
				}
				assigns[e.var] = l_Undef;
				//changed = true;
			}
			trail.shrink(trail.size() - stop);
			trail_lim.shrink(trail_lim.size() - level);
			assert(trail_lim.size() == level);
			

			requiresPropagation |= altered_bvs.size();


		}
		analysis_trail_pos = trail.size()-1;


		
	};

	Lit decideTheory() {

		return lit_Undef;
	}
	
	void backtrackUntil(Lit p) {
		rewind_trail_pos(trail.size());
		assert(value(p)!=l_False);
		if(value(p)!=l_True)
			return;
		int n_backtrack=0;
		assert(value(p)==l_True);
		int i = trail.size() - 1;
		for (; i >= 0; i--) {
			Assignment e = trail[i];
			if (var(p) == e.var) {
				assert(sign(p) != e.assign);
				break;
			}
			if (e.isComparator) {
				int cID = e.bvID;
				assert(cID>=0);
				ComparisonID & c = comparisons[cID];
				int bvID = c.bvID;
				//if the bitvector's cause was this comparison, then we need to update the bitvector now.
				under_approx[bvID]=e.previous_under;
				over_approx[bvID]=e.previous_over;
				under_causes[bvID]=e.prev_under_cause;
				over_causes[bvID]=e.prev_over_cause;
				if(!alteredBV[bvID]){
					alteredBV[bvID]=true;
					altered_bvs.push(bvID);
				}
			} else {

				int bvID = e.bvID;
				under_approx[bvID]=e.previous_under;
				over_approx[bvID]=e.previous_over;
				under_causes[bvID]=e.prev_under_cause;
				over_causes[bvID]=e.prev_over_cause;
				if(!alteredBV[bvID]){
					alteredBV[bvID]=true;
					altered_bvs.push(bvID);
				}
			}
			assigns[e.var] = l_Undef;
			//changed = true;
		}
		
		trail.shrink(trail.size() - (i + 1));
		analysis_trail_pos = trail.size()-1;
		//if(i>0){
		requiresPropagation = true;

	};


	void newDecisionLevel() {
		trail_lim.push(trail.size());
	}
	;

	void buildReason(Lit p, vec<Lit> & reason,CRef marker) {
		static int iter = 0;
		++iter;
		assert(value(p)!=l_False);

		assert(marker != CRef_Undef);
		int pos = CRef_Undef - marker;
		//int d = marker_map[pos];
		//double initial_start = rtime(1);
		double start = rtime(1);

		//if the reason is being constructed eagerly, then p won't be assigned yet, and so wont be on the trail, so we skip this.
		backtrackUntil(p);
		assert(value(p)!=l_False);
		//now that we have backtracked, we need to update the under/over approximations.
		//there is likely room to improve this, so that only relevant bitvectors are updated, but it might be
		//complicated to do so... Note that there ARE cases where bitvectors beyond the one directly responsible for p may need to be updated:
		//if p is caused by the addition of two other bitvectors, for example, then they both need to be updated.
	/*	for(int bvID = 0;bvID<bitvectors.size();bvID++){
			updateApproximations(bvID);
		}*/

		if (marker == comparisonprop_marker) {
			reason.push(p);
			Var v = var(p);
			int bvID = getbvID(v);

			assert(isComparisonVar(v));
			int comparisonID = getComparisonID(v);

			Comparison op = comparisons[comparisonID].op();
			if (comparisons[comparisonID].compareID<0){
				if(sign(p)){
					op=-op;
				}
				buildValueReason(op,bvID,comparisons[comparisonID].w,reason);
			}else{
				int compareBV = comparisons[comparisonID].compareID;
				if(sign(p)){
					op=-op;
				}
				buildValueReasonBV(op,bvID,compareBV,reason);
			}


		} else if (marker == bvprop_marker) {
			Var v = var(p);

			assert(!isComparisonVar(v));
			int bvID = getbvID(v);

			reason.push(p);

			Weight underApprox = under_approx[bvID];
			Weight overApprox = over_approx[bvID];
			static int iterv =0;
			++iterv;

			assert(under_approx>=0); assert(overApprox>=0);
			vec<Lit> & bv = bitvectors[bvID];

			int bitpos=-1;
			for(int i = bv.size()-1;i>=0;i--){
				lbool val = value(bv[i]);
				Lit l = bv[i];
				if(var(bv[i])==v){
					bitpos=i;
					break;
				}
			}
			Weight bit = 1<<bitpos;
		/*	if(underApprox==0){
				assert(overApprox<underApprox+bit);
				overApprox=bit-1;
			}
			if(overApprox==(1L<<bv.size())-1){
				assert(underApprox> overApprox-bit);
				underApprox=(1L<<bv.size())-1 - bit+1;
			}*/
			assert(bitpos>=0);
			if(!sign(p)){
				//if this bit is true, then the reason is because the overapprox, minus this bit, would be less than the underapprox.
				//hence either the underapprox must decrease, or the overapprox must increase.

				buildValueReason(Comparison::leq,bvID,overApprox,reason);
				buildValueReason(Comparison::geq,bvID,underApprox,reason);
			}else{
				//if this bit is false, then the reason is because the underapprox, plus this bit, would be greater than the overapprox.
				//hence either the underapprox must decrease, or the overapprox must increase.
				buildValueReason(Comparison::leq,bvID,overApprox,reason);
				buildValueReason(Comparison::geq,bvID,underApprox,reason);
			}



		}  else {
			assert(false);
		}

		toSolver(reason);
		double finish = rtime(1);
		stats_reason_time += finish - start;
		stats_num_reasons++;
		//stats_reason_initial_time+=start-initial_start;
		if(reason.size()<2){
			int a=1;
		}
	}


	void preprocess() {

	}
	void setLiteralOccurs(Lit l, bool occurs) {
		/*if (isEdgeVar(var(l))) {
			//don't do anything
		} else {
			//this is a graph property detector var
			if (!sign(l) && vars[var(l)].occursPositive != occurs)
				detectors[getDetector(var(l))]->setOccurs(l, occurs);
			else if (sign(l) && vars[var(l)].occursNegative != occurs)
				detectors[getDetector(var(l))]->setOccurs(l, occurs);
		}*/
		
	}
	void enqueueTheory(Lit l) {
		int bvID = getbvID(var(l));
		enqueueTheory(l,under_approx[bvID],over_approx[bvID],under_approx[bvID],over_approx[bvID], under_causes[bvID],over_causes[bvID], under_causes[bvID],over_causes[bvID]);
	}
	void enqueueTheory(Lit l, Weight prev_under, Weight prev_over, Weight new_under, Weight new_over,Cause prev_under_cause,Cause prev_over_cause,Cause new_under_cause,Cause new_over_cause) {
		Var v = var(l);
		rewind_trail_pos(trail.size());
		int lev = level(v);
		while (lev > trail_lim.size()) {
			newDecisionLevel();
		}
		
		if (assigns[var(l)] != l_Undef) {
			return;			//this is already enqueued.
		}
		assert(assigns[var(l)]==l_Undef);
		assigns[var(l)] = sign(l) ? l_False : l_True;
		requiresPropagation = true;
		//printf("enqueue %d\n", dimacs(l));
		
#ifndef NDEBUG
		{
			for (int i = 0; i < trail.size(); i++) {
				assert(trail[i].var != v);
			}
		}
#endif
		
		

		if (!isComparisonVar(var(l))) {
			
			int bvID = getbvID(v);
			trail.push( { false, !sign(l),bvID, v,prev_under, prev_over, new_under, new_over,prev_under_cause,prev_over_cause,new_under_cause,new_over_cause});
			analysis_trail_pos=trail.size()-1;
			if(!alteredBV[bvID]){
				alteredBV[bvID]=true;
				if (altered_bvs.size()==0)
					altered_bvs.push(bvID);
				else{
					//preserve last element in alteredbvs, in case this enqueue happened during propagation
					int lastID = altered_bvs.last();
					altered_bvs.last()=bvID;
					assert(altered_bvs.last()==bvID);
					altered_bvs.push(lastID);
					assert(altered_bvs.last()==lastID);
				}

			}

		} else {

			int bvID = getbvID(var(l));
			int comparisonID = getComparisonID(var(l)); //v-min_edge_var;
			//status.comparisonAltered(bvID, comparisonID);
			trail.push( { true, !sign(l),comparisonID, v,prev_under, prev_over, new_under, new_over,prev_under_cause,prev_over_cause,new_under_cause,new_over_cause });
			analysis_trail_pos=trail.size()-1;
			//trail.push( { true, !sign(l),edgeID, v });
			if(!alteredBV[bvID]){
				alteredBV[bvID]=true;
				if (altered_bvs.size()==0)
					altered_bvs.push(bvID);
				else{
					//preserve last element in alteredbvs, in case this enqueue happened during propagation
					int lastID = altered_bvs.last();
					altered_bvs.last()=bvID;
					assert(altered_bvs.last()==bvID);
					altered_bvs.push(lastID);
					assert(altered_bvs.last()==lastID);
				}

			}
		}
	}
	;
private:
	Weight lowest(int bvID);
	Weight highest(int bvID);
	Weight refine_lbound(int bvID, Weight bound, Var ignore_bit=var_Undef);
	Weight refine_ubound(int bvID, Weight bound, Var ignore_bit=var_Undef);
	Weight refine_ubound_check(int bvID, Weight bound, Var ignore_bit);
	Weight refine_lbound_check(int bvID, Weight bound, Var ignore_bit);
	void dbg_evaluate(int bvID, int pos,vec<Weight> & vals,Weight val);
public:
	bool updateApproximations(int bvID, int ignoreCID=-1, Var ignore_bv=var_Undef){
		if(isConst(bvID))
			return false;
		if(bvID==34393){
			int a=1;
		}
		statis_bv_updates++;
		static int iter = 0;
		++iter;
		if(iter==45){
			int a = 1;
		}
#ifndef NDEBUG
/*		for(int i = 0;i<vars.size();i++){
			if(value(i)==l_True){
				std::cout << "1";
			}else if (value(i)==l_False){
				std::cout << "0";
			}else{
				std::cout << "x";
			}
		}
		std::cout<<"\n";*/
#endif
		Weight under_old = under_approx[bvID];
		Weight over_old = over_approx[bvID];
		vec<Lit> & bv = bitvectors[bvID];
		Weight under_new=0;
		Weight over_new=0;
		under_causes[bvID].clear();
		over_causes[bvID].clear();

		//under_approx[bvID]=0;
		//over_approx[bvID]=0;
		for(int i = 0;i<bv.size();i++){
			if(var(bv[i])==ignore_bv){
				Weight bit = 1<<i;
				over_new+=bit;
				continue;
			}
			lbool val = value(bv[i]);
			if(val==l_True){
				Weight bit = 1<<i;
				under_new+=bit;
				over_new+=bit;
				under_causes[bvID].cause_is_bits=true;
			}else if (val==l_False){
				over_causes[bvID].cause_is_bits=true;
			}else{
				Weight bit = 1<<i;
				over_new+=bit;

			}
		}

		if(additions[bvID].hasAddition()){
			int aID = additions[bvID].aID;
			int bID = additions[bvID].bID;
			assert(aID<bvID);
			assert(bID<bvID);
			Weight under = under_approx[aID] +  under_approx[bID];
			Weight over = over_approx[aID] +  over_approx[bID];

			if(under >under_new){
				under_new=under;
				under_causes[bvID].clear();
				under_causes[bvID].cause_is_addition=true;
			}
			if(over<over_new){
				over_new=over;
				over_causes[bvID].clear();
				over_causes[bvID].cause_is_addition=true;
			}
		}

		//also need to iterate through the additions that this bv is an argument of...
		for(int i = 0;i<addition_arguments[bvID].size();i++){
			int other_argID = addition_arguments[bvID][i].other_argID;
			int sumID = addition_arguments[bvID][i].sumID;
			assert(other_argID>=0);assert(sumID>=0);
			assert(additions[sumID].hasAddition());
			assert((additions[sumID].aID==bvID && additions[sumID].bID==other_argID ) || (additions[sumID].bID==bvID && additions[sumID].aID==other_argID ) );
			//assert((other_argID!=bvID &&   under_approx[sumID] >=  under_approx[other_argID] + under_old  ) || (other_argID==bvID &&   under_approx[sumID] >= under_old + under_old  ));
			//assert((other_argID!=bvID &&  over_approx[sumID] <=  over_approx[other_argID] + over_old ) || (other_argID==bvID &&  over_approx[sumID] <=  over_old + over_old ));
			Weight under = under_approx[sumID] -  over_approx[other_argID];
			Weight over = over_approx[sumID] -  under_approx[other_argID];

			if(under >under_new){
				under_new=under;
				under_causes[bvID].clear();
				under_causes[bvID].cause_is_addition_arg=i;
			}
			if(over<over_new){
				over_new=over;
				over_causes[bvID].clear();
				over_causes[bvID].cause_is_addition_arg=i;
			}
		}

		for(int i = compares[bvID].size()-1;i>=0;i--){
			int cID = compares[bvID][i];
			if(cID==ignoreCID){
				continue;
			}
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			bool setOver=false;
			Weight w = c.w;
			assert(!c.bvCompare());
			switch(op){
				case Comparison::lt:
					if(value( c.l)==l_True && over_new>=w){
						if(w-1>=getUnderApprox(bvID,true)){
							over_new=w-1;
							setOver=true;
						 }
					}
					break;
				case Comparison::leq:
					if(value( c.l)==l_True && over_new>w){
						if(w>=getUnderApprox(bvID,true)){
							over_new=w;
							setOver=true;
						 }
					}
					break;
				case Comparison::gt:
					if (value(c.l)==l_False && over_new>w){
						if(w>=getUnderApprox(bvID,true)){
							over_new=w;
							setOver=true;
						 }
					}
					break;
				case Comparison::geq:
				default:
					if (value(c.l)==l_False && over_new>=w){
						if(w-1>=getUnderApprox(bvID,true)){
							over_new=w-1;
							setOver=true;
						 }
					}
					break;
			}

			if(setOver){
				over_causes[bvID].clear();
				over_causes[bvID].comparison_cause=cID;
			}
		}


		for(int cID:compares[bvID]){
			if(cID==ignoreCID){
				continue;
			}
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();
			bool setUnder=false;
			assert(!c.bvCompare());
			Weight w = c.w;

			switch(op){
				case Comparison::lt:
					 if (value(c.l)==l_False && under_new<w){
						 if(w<=getOverApprox(bvID,true)){
							 under_new=w;
							 setUnder=true;
						 }
					}
					break;
				case Comparison::leq:
					 if (value(c.l)==l_False && under_new<=w){
						 if(w<=getOverApprox(bvID,true)){
							 under_new=w+1;
							setUnder=true;
						 }
					}
					break;
				case Comparison::gt:
					if(value( c.l)==l_True && under_new<=w){
						 if(w+1<=getOverApprox(bvID,true)){
							 under_new=w+1;
							 setUnder=true;
						 }
					}
					break;
				case Comparison::geq:
				default:
					if(value( c.l)==l_True && under_new<w){
						 if(w<=getOverApprox(bvID,true)){
							 under_new=w;
							 setUnder=true;
						 }
					}
					break;
			}

			if(setUnder){
				under_causes[bvID].clear();
				under_causes[bvID].comparison_cause=cID;
			}

		}


		for(int i = bvcompares[bvID].size()-1;i>=0;i--){
			int cID = bvcompares[bvID][i];
			if(cID==ignoreCID){
				continue;
			}
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			bool setOver=false;

			assert(c.bvCompare());
			Weight w = over_approx[c.compareID];
			switch(op){
				case Comparison::lt:
					if(value( c.l)==l_True && over_new>=w){
						if(w-1>=getUnderApprox(bvID,true)){
							over_new=w-1;
							setOver=true;
						}
					}
					break;
				case Comparison::leq:
					if(value( c.l)==l_True && over_new>w){
						if(w>=getUnderApprox(bvID,true)){
							over_new=w;
							setOver=true;
						}
					}
					break;
				case Comparison::gt:
					if (value(c.l)==l_False && over_new>w){
						if(w>=getUnderApprox(bvID,true)){
							over_new=w;
							setOver=true;
						}
					}
					break;
				case Comparison::geq:
				default:
					if (value(c.l)==l_False && over_new>=w){
						if(w-1>=getUnderApprox(bvID,true)){
							over_new=w-1;
							setOver=true;
						}
					}
					break;
			}

			if(setOver){
				over_causes[bvID].clear();
				over_causes[bvID].comparison_cause=cID;
			}
		}

		for(int cID:bvcompares[bvID]){
			if(cID==ignoreCID){
				continue;
			}
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();
			bool setUnder=false;
			assert(c.bvID>=0);
			Weight w = under_approx[c.compareID];

			switch(op){
				case Comparison::lt:
					 if (value(c.l)==l_False && under_new<w){
						 if(w<=getOverApprox(bvID,true)){
							 under_new=w;
							 setUnder=true;
						 }
					}
					break;
				case Comparison::leq:
					 if (value(c.l)==l_False && under_new<=w){
						 if(w+1<=getOverApprox(bvID,true)){
							under_new=w+1;
							setUnder=true;
						 }
					}
					break;
				case Comparison::gt:
					if(value( c.l)==l_True && under_new<=w){
						 if(w+1<=getOverApprox(bvID,true)){
							 under_new=w+1;
							 setUnder=true;
						 }
					}
					break;
				case Comparison::geq:
				default:
					if(value( c.l)==l_True && under_new<w){
						 if(w<=getOverApprox(bvID,true)){
							 under_new=w;
							 setUnder=true;
						 }
					}
					break;
			}

			if(setUnder){
				under_causes[bvID].clear();
				under_causes[bvID].comparison_cause=cID;
			}

		}

		Weight refined_over = refine_lbound(bvID, over_new);
		if(refined_over>-1 && refined_over< over_new){
			std::cout<< "Refined overapprox for bv " << bvID << " from " << over_new << " to " << refined_over << "\n";
			over_new=refined_over;
			over_causes[bvID].refined_cause=true;
		}
		Weight refined_under = refine_ubound(bvID, under_new);
		if(refined_under>-1  && refined_under> under_new){
			std::cout<< "Refined underapprox for bv " << bvID << " from " << under_new<< " to " << refined_under << "\n";
			under_new=refined_under;
			under_causes[bvID].refined_cause=true;
		}

		int width = bitvectors[bvID].size();
		Weight max_val = ((1L)<<width)-1;
		if(under_new>over_approx0[bvID]){
			under_new=over_approx0[bvID];
		}
		if(over_new>over_approx0[bvID]){
			over_new=over_approx0[bvID];
		}
		if(under_new<under_approx0[bvID]){
			under_new=under_approx0[bvID];
		}
		if(over_new<under_approx0[bvID]){
			over_new=under_approx0[bvID];
		}

		under_approx[bvID]=under_new;
		over_approx[bvID]=over_new;

		if(decisionLevel()==0){
			under_approx0[bvID]=under_approx[bvID];
			over_approx0[bvID]=over_approx[bvID];
			if (under_approx[bvID]==over_approx[bvID]){
				assert(!bvconst[bvID]);
				bvconst[bvID]=true;//this bitvector is a known constant value in the formula.
				n_consts++;
			}
		}

#ifndef NDEBUG
		static int bound_num=0;
		printf("learnt bound ");
		printf(" %d <= ", bvID);
		std::cout<<over_approx[bvID] << " ";

		for (int i = 0;i<nVars();i++){
				if(value(i)!=l_Undef){
					Lit l = mkLit(i,value(i)==l_True);
					assert(value(l)==l_False);
					printf(" %d",dimacs(toSolver(l)));
				}
			}

		printf(" 0\n");
		++bound_num;
		printf("learnt bound ");
		printf(" %d >= ", bvID);
		std::cout<<under_approx[bvID] << " ";

		for (int i = 0;i<nVars();i++){
				if(value(i)!=l_Undef){
					Lit l = mkLit(i,value(i)==l_True);
					assert(value(l)==l_False);
					printf(" %d",dimacs(toSolver(l)));
				}
			}

		printf(" 0\n");
		if(++bound_num>=89){
			int  a=1;
		}
		printf("%d\n", bound_num);
#endif

		return 	(under_old != under_approx[bvID]) || (over_old != over_approx[bvID]);//return whether either weight has changed.
	}

	Weight & getUnderApprox(int bvID, bool level0=false){
		if(level0){
			return under_approx0[bvID];
		}else{
			return under_approx[bvID];
		}
	}
	Weight & getOverApprox(int bvID, bool level0=false){
		if(level0){
			return over_approx0[bvID];
		}else{
			return over_approx[bvID];
		}
	}

	vec<Lit> & getBits(int bvID){
		return bitvectors[bvID];
	}

	void buildTrivialClause(vec<Lit> & conflict){
		for (int i = 0;i<nVars();i++){
			if(value(i)!=l_Undef){
				Lit l = mkLit(i,value(i)==l_True);
				assert(value(l)==l_False);
				conflict.push(l);
			}
		}
	}

	bool checkApproxUpToDate(int bvID, Weight * under_store=nullptr,Weight*over_store=nullptr){
#ifndef NDEBUG
		vec<Lit> & bv = bitvectors[bvID];
		Weight under =0;
		Weight over=0;
		for(int i = 0;i<bv.size();i++){
			lbool val = value(bv[i]);
			if(val==l_True){
				Weight bit = 1<<i;
				under+=bit;
				over+=bit;
			}else if (val==l_False){

			}else{
				Weight bit = 1<<i;
				over+=bit;
			}
		}

		if(additions[bvID].hasAddition()){
			int aID = additions[bvID].aID;
			int bID = additions[bvID].bID;
			assert(aID<bvID);
			assert(bID<bvID);
			Weight underadd = under_approx[aID] +  under_approx[bID];
			Weight overadd = over_approx[aID] +  over_approx[bID];
			if(underadd >under){
				under=underadd;
			}
			if(overadd<over){
				over=overadd;
			}
		}

		//also need to iterate through the additions that this bv is an argument of...
		for(int i = 0;i<addition_arguments[bvID].size();i++){
			int other_argID = addition_arguments[bvID][i].other_argID;
			int sumID = addition_arguments[bvID][i].sumID;

			Weight under_add = under_approx[sumID] -  over_approx[other_argID];
			Weight over_add = over_approx[sumID] -  under_approx[other_argID];

			if(under_add >under){
				under=under_add;
			}
			if(over_add<over){
				over=over_add;
			}
		}

		for(int cID:compares[bvID]){
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			lbool val = value(c.l);
			if(!c.bvCompare()){
				switch(op){
					case Comparison::lt:
						if(value( c.l)==l_True && over>=c.w){
							over=c.w-1;
						}else if (value(c.l)==l_False && under<c.w){
							under=c.w;
						}
						break;
					case Comparison::leq:
						if(value( c.l)==l_True && over>c.w){
							over=c.w;
						}else if (value(c.l)==l_False && under<=c.w){
							under=c.w+1;
						}
						break;
					case Comparison::gt:
						if(value( c.l)==l_True && under<=c.w){
							under=c.w+1;
						}else if (value(c.l)==l_False && over>c.w){
							over=c.w;
						}
						break;
					case Comparison::geq:
					default:
						if(value( c.l)==l_True && under<c.w){
							under=c.w;
						}else if (value(c.l)==l_False && over>=c.w){
							over=c.w-1;
						}
						break;
				}
			}else{


			}
		}
		for(int cID:bvcompares[bvID]){
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			lbool val = value(c.l);
			if(c.bvCompare()){
				Weight under_w = under_approx[c.compareID];
				Weight over_w = over_approx[c.compareID];

				switch(op){

					case Comparison::lt:
						if(value( c.l)==l_True && over>=over_w){
							over=over_w-1;
						}else if (value(c.l)==l_False && under<under_w){
							under=under_w;
						}
						break;
					case Comparison::leq:
						if(value( c.l)==l_True && over>over_w){
							over=over_w;
						}else if (value(c.l)==l_False && under<=under_w){
							under=under_w+1;
						}
						break;
					case Comparison::gt:
						if(value( c.l)==l_True && under<=under_w){
							under=under_w+1;
						}else if (value(c.l)==l_False && over>over_w){
							over=over_w;
						}
						break;
					case Comparison::geq:
					default:
						if(value( c.l)==l_True && under<under_w){
							under=under_w;
						}else if (value(c.l)==l_False && over>=over_w){
							over=over_w-1;
						}
						break;
				}
			}else{


			}
		}
		Weight refined_over = refine_lbound(bvID, over);
		if(refined_over>-1 && refined_over< over){

			over=refined_over;
		}
		Weight refined_under = refine_ubound(bvID,under);
		if(refined_under>-1  && refined_under> under){

			under=refined_under;
		}

		int width = bitvectors[bvID].size();
		Weight max_val = (1L<<width)-1;
		if(under>over_approx0[bvID]){
			under=over_approx0[bvID];
		}
		if(over>over_approx0[bvID]){
			over=over_approx0[bvID];
		}
		if(under<under_approx0[bvID]){
			under=under_approx0[bvID];
		}
		if(over<under_approx0[bvID]){
			over=under_approx0[bvID];
		}
		assert(under==under_approx[bvID]);
		assert(over==over_approx[bvID]);
		assert(under_approx0[bvID]<=under_approx[bvID]);
		assert(over_approx0[bvID]>=over_approx[bvID]);
		if(under_store){
			(*under_store)=under;
		}
		if(over_store){
			(*over_store)=over;
		}
#endif
		return true;
	}

	bool dbg_synced(){
#ifndef NDEBUG
		for(Var v = 0;v<nVars();v++){
			assert(value(v)==dbg_value(v));
		}
#endif
		return true;
	}

	bool checkAllApproxUpToDate(){
#ifndef NDEBUG
		for (int i = 0;i< bitvectors.size();i++){
			assert(checkApproxUpToDate(i));
		}
#endif
		return true;
	}

/*	bool dbg_uptodate(){
#ifndef NDEBUG
		assert(checkAllApproxUpToDate());
		for(int cID = 0;cID<comparisons.size();cID++){
			ComparisonID & c = comparisons[cID];
			int bvID = c.bvID;
			Comparison op =c.op();
			if(!c.bvCompare()){
				Weight w = c.w;
				if(op==Comparison::lt && getOverApprox(bvID)<w){

				}

			}else{

			}

		}
#endif
		return true;
	}*/

	bool propagateTheory(vec<Lit> & conflict) {
		return  propagateTheory(conflict, true);
	}

	bool propagateTheory(vec<Lit> & conflict, bool propagate) {
		static int realprops = 0;
		stats_propagations++;

		if (!requiresPropagation) {
			stats_propagations_skipped++;
			assert(dbg_uptodate());
			return true;
		}
		rewind_trail_pos(trail.size());
		if(++realprops==11){
			int a =1;
		}
		//printf("bv prop %d\n",stats_propagations);
		if(stats_propagations==110){
			int a =1;
		}
		bool any_change = false;
		double startproptime = rtime(1);
		//static vec<int> detectors_to_check;
		
		conflict.clear();
		

		while(altered_bvs.size()){
			int bvID = altered_bvs.last();
		//for(int bvID = 0;bvID<bitvectors.size();bvID++){
			assert(alteredBV[bvID]);
			Weight  underApprox_prev = under_approx[bvID];
			Weight  overApprox_prev = over_approx[bvID];
			Cause prev_under_cause = over_causes[bvID];
			Cause prev_over_cause = over_causes[bvID];
			bool changed = updateApproximations(bvID);

			Weight & underApprox = under_approx[bvID];
			Weight & overApprox = over_approx[bvID];
			printf("iter %d, bv %d, under ",realprops , bvID); //: %d, over %d\n", bvID, underApprox,overApprox);
			std::cout<<underApprox << " over ";
			std::cout<<overApprox << "\n";
			fflush(stdout);
			vec<Lit> & bv = bitvectors[bvID];
			Weight under =0;
			Weight over=(1L<<bv.size())-1;
			//for(int i = 0;i<bv.size();i++){
			for(int i = bv.size()-1;i>=0;i--){
				Weight bit = 1<<i;
				lbool val = value(bv[i]);
				Lit l = bv[i];
				if(val==l_True){
					under+=bit;
					if(under>overApprox){

							//this is a conflict
							for(int j = bv.size()-1;j>=i;j--){
								Weight bit = 1<<j;
								lbool val = value(bv[j]);
								if(val==l_True){
									conflict.push(~bv[j]);
								}
							}
							stats_num_conflicts++;stats_bit_conflicts++;
							double startconftime = rtime(2);
							buildValueReason(Comparison::leq,bvID,overApprox,conflict);
							toSolver(conflict);
							stats_conflict_time+=rtime(2)-startconftime;
							return false;

					}
				}else if (val==l_False){
					over-=bit;
					if(over<underApprox){

							//this is a conflict. Either this bit, or any previously assigned false bit, must be true, or the underapprox must be larger than it currently is.
							//is this really the best way to handle this conflict?
							for(int j = bv.size()-1;j>=i;j--){
								Weight bit = 1<<j;
								lbool val = value(bv[j]);
								if(val==l_False){
									conflict.push(bv[j]);
								}
							}
							double startconftime = rtime(2);
							stats_num_conflicts++;stats_bit_conflicts++;
							buildValueReason(Comparison::geq,bvID,underApprox,conflict);
							toSolver(conflict);
							stats_conflict_time+=rtime(2)-startconftime;
							return false;

					}
				}else{
					if(propagate){
						if(over-bit < underApprox){
							enqueue(l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, bvprop_marker);
							under+=bit;
						}else if (under+bit>overApprox){
							enqueue(~l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, bvprop_marker);
							over-=bit;
						}
					}
				}
			}

			changed|=updateApproximations(bvID);//the bit assignment updates above can force a more precise over or under approximation.

			if(additions[bvID].hasAddition()){
				int aID = additions[bvID].aID;
				int bID = additions[bvID].bID;
				assert(aID<bvID);
				assert(bID<bvID);
				Weight under = under_approx[aID] +  under_approx[bID];
				Weight over = over_approx[aID] +  over_approx[bID];
				int width = bitvectors[bvID].size();
				Weight max_val = ((1L)<<width)-1;
				if(under>max_val){
					under=max_val;
				}
				if(over>max_val){
					over=max_val;
				}

				if(underApprox>over){
					//then we have a conflict
					double startconftime = rtime(2);
					stats_num_conflicts++;stats_addition_conflicts++;
					buildAdditionReason(bvID,conflict);
					toSolver(conflict);
					stats_conflict_time+=rtime(2)-startconftime;
					return false;
				}else if (overApprox<under){
					double startconftime = rtime(2);
					stats_num_conflicts++;stats_addition_conflicts++;
					buildAdditionReason(bvID,conflict);
					toSolver(conflict);
					stats_conflict_time+=rtime(2)-startconftime;
					return false;
				}

				//this check may be especially important when either aID or bID is really a constant...
				if(( underApprox - over_approx[bID]> under_approx[aID]) || ( overApprox - under_approx[bID]< over_approx[aID])){
					//the other bv needs to be updated
					if(!alteredBV[aID]){
						alteredBV[aID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=aID;
						assert(altered_bvs.last()==aID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}
				if(( underApprox - over_approx[aID]> under_approx[bID]) || ( overApprox - under_approx[aID]< over_approx[bID])){
					//the other bv needs to be updated
					if(!alteredBV[bID]){
						alteredBV[bID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=bID;
						assert(altered_bvs.last()==bID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}
			}

			//also need to iterate through the additions that this bv is an argument of...
			for(int i = 0;i<addition_arguments[bvID].size();i++){
				int other_argID = addition_arguments[bvID][i].other_argID;
				int sumID = addition_arguments[bvID][i].sumID;
				assert(other_argID>=0);assert(sumID>=0);
				assert(additions[sumID].hasAddition());
				assert((additions[sumID].aID==bvID && additions[sumID].bID==other_argID ) || (additions[sumID].bID==bvID && additions[sumID].aID==other_argID ) );

				Weight under = under_approx[sumID] -  over_approx[other_argID];
				Weight over = over_approx[sumID] -  under_approx[other_argID];

				if(underApprox>over){
					//then we have a conflict
					double startconftime = rtime(2);
					stats_num_conflicts++;stats_addition_conflicts++;
					buildAdditionArgReason(bvID,i,conflict);
					toSolver(conflict);
					stats_conflict_time+=rtime(2)-startconftime;
					return false;
				}else if (overApprox<under){
					double startconftime = rtime(2);
					stats_num_conflicts++;stats_addition_conflicts++;
					buildAdditionArgReason(bvID,i,conflict);
					toSolver(conflict);
					stats_conflict_time+=rtime(2)-startconftime;
					return false;
				}
				//this check may be especially important when either aID or bID is really a constant...
				if(( underApprox + under_approx[other_argID]> under_approx[sumID]) || ( overApprox + over_approx[other_argID]< over_approx[sumID])){
					//the other bv needs to be updated

					if(!alteredBV[sumID]){
						//exit(8);
						alteredBV[sumID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=sumID;
						assert(altered_bvs.last()==sumID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}

				if( ( under_approx[sumID]  - overApprox> under_approx[other_argID]) || (over_approx[sumID] - underApprox< over_approx[other_argID])){
					//the other bv needs to be updated

					if(!alteredBV[other_argID]){
						//exit(8);
						alteredBV[other_argID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=other_argID;
						assert(altered_bvs.last()==other_argID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}
			}


			vec<int> & compare = compares[bvID];
			//update over approx lits
			for(int i = 0;i<compare.size();i++){
				int cID = compare[i];
				ComparisonID & c = comparisons[cID];
				Comparison op = c.op();
				Weight & to = c.w;
				Lit l =  c.l;

				if((op==Comparison::lt && overApprox<to) ||
						(op==Comparison::leq && overApprox<=to)){
					if(value(l)==l_True){
						//do nothing

					}else if (value(l)==l_False){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_compare_conflicts++;
						assert(value(l)==l_False);
						assert(dbg_value(l)==l_False);
						conflict.push(l);
						buildValueReason(op,bvID,to,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}else if((op==Comparison::gt && overApprox<=to) ||
						(op==Comparison::geq && overApprox<to)){
					if(value(l)==l_True){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_compare_conflicts++;
						conflict.push(~l);
						buildValueReason(-op,bvID,to,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}else if (value(l)==l_False){

					}else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(~l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}
			}


			for(int i=compare.size()-1;i>=0;i--){
				int cID = compare[i];
				ComparisonID & c = comparisons[cID];
				assert(!c.bvCompare());
				Comparison op = c.op();
				Weight & to = c.w;
				Lit l =  c.l;

				if((op==Comparison::lt && underApprox>=to) ||
						(op==Comparison::leq && underApprox>to)){
					if(value(l)==l_True){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_compare_conflicts++;
						conflict.push(~l);
						buildValueReason(-op,bvID,to,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}else if (value(l)==l_False){
						//do nothing

					}else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(~l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}else if((op==Comparison::gt && underApprox>to) ||
						(op==Comparison::geq && underApprox>=to)){
					if(value(l)==l_True){

					}else if (value(l)==l_False){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_compare_conflicts++;
						conflict.push(l);
						buildValueReason(op,bvID,to,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}
			}

			//comparisons to bitvectors.
			vec<int> & bvcompare = bvcompares[bvID];

			for(int i = 0;i<bvcompare.size();i++){
				int cID = bvcompare[i];
				ComparisonID & c = comparisons[cID];
				assert(c.bvCompare());
				Comparison op = c.op();
				int compareID = c.compareID;
				assert(compareID>=0);
				Lit l =  c.l;
				lbool val = value(l);
				if(value(l)==l_False){
					l=~l;
					op=-op;
				}
				assert(value(l)!=l_False);
				Weight & under_compare = under_approx[compareID];

				if((op==Comparison::lt && overApprox<under_compare) ||
						(op==Comparison::leq && overApprox<=under_compare)){
					if(value(l)==l_True){
						//do nothing

					}/*else if (value(l)==l_False){

						assert(value(l)==l_False);
						assert(dbg_value(l)==l_False);
						conflict.push(l);
						buildValueReasonBV(op,bvID,compareID,conflict);
						toSolver(conflict);
						return false;
					}*/else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}else if((op==Comparison::gt && overApprox<=under_compare) ||
						(op==Comparison::geq && overApprox<under_compare)){
					if(value(l)==l_True){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_bv_compare_conflicts++;
						conflict.push(~l);
						buildValueReasonBV(-op,bvID,compareID,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}/*else if (value(l)==l_False){

					}*/else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(~l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}
				if(value(l)==l_True &&((op==Comparison::lt && underApprox>=under_compare) ||
						(op==Comparison::leq && underApprox>under_compare))){
					//the other bv needs to be updated
					if(!alteredBV[compareID]){
						alteredBV[compareID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=compareID;
						assert(altered_bvs.last()==compareID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}
			}


			for(int i=bvcompare.size()-1;i>=0;i--){
				int cID = bvcompare[i];
				ComparisonID & c = comparisons[cID];
				Comparison op = c.op();
				int compareID = c.compareID;
				assert(compareID>=0);
				Lit l =  c.l;
				lbool val = value(l);
				if(value(l)==l_False){
					l=~l;
					op=-op;
				}
				assert(value(l)!=l_False);
				//updateApproximations(compareID);
				Weight & over_compare = over_approx[compareID];
				if((op==Comparison::lt && underApprox>=over_compare) ||
						(op==Comparison::leq && underApprox>over_compare)){
					if(value(l)==l_True){
						double startconftime = rtime(2);
						stats_num_conflicts++;stats_bv_compare_conflicts++;
						conflict.push(~l);
						buildValueReasonBV(-op,bvID,compareID,conflict);
						toSolver(conflict);
						stats_conflict_time+=rtime(2)-startconftime;
						return false;
					}/*else if (value(l)==l_False){
						//do nothing

					}*/else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(~l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}else if((op==Comparison::gt && underApprox>over_compare) ||
						(op==Comparison::geq && underApprox>=over_compare)){
					if(value(l)==l_True){

					}/*else if (value(l)==l_False){

						conflict.push(l);
						buildValueReasonBV(op,bvID,compareID,conflict);
						toSolver(conflict);
						return false;
					}*/else {
						if(propagate){
							assert(value(l)==l_Undef);
							enqueue(l,bvID,underApprox_prev,overApprox_prev,prev_under_cause,prev_over_cause, comparisonprop_marker);
						}
					}
				}
				//we can also update the other bv's approximation, possibly:
				if(value(l)==l_True &&( (op==Comparison::gt && overApprox<=over_compare) ||
						(op==Comparison::geq && overApprox<over_compare))){
					//the other bv needs to be updated
					if(!alteredBV[compareID]){
						alteredBV[compareID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=compareID;
						assert(altered_bvs.last()==compareID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}

			}
			if(propagate){
				if(hasTheory(bvID))
					getTheory(bvID)->enqueueBV(bvID);//only enqueue the bitvector in the subtheory _after_ it's approximation has been updated!
			}

	/*		if(changed){
				//then any additions this is an argument of need to be updated.
				for(int changedID: cause_set[bvID]){
					if(!alteredBV[changedID]){
						alteredBV[changedID]=true;
						assert(altered_bvs.last()==bvID);
						altered_bvs.last()=changedID;
						assert(altered_bvs.last()==changedID);
						altered_bvs.push(bvID);
						assert(altered_bvs.last()==bvID);
					}
				}
			}*/

			assert(altered_bvs.last()==bvID);
			assert(alteredBV[bvID]==true);
			altered_bvs.pop();
			alteredBV[bvID]=false;
		}
		
		requiresPropagation = false;
		double elapsed = rtime(1) - startproptime;
		propagationtime += elapsed;
		assert(dbg_uptodate());
		if(first_propagation){
			first_propagation=false;
			n_starting_consts=n_consts;
		}
		return true;
	};

	Weight ceildiv(Weight a, Weight b);

	void buildAdditionArgReason(int bvID, int argindex, vec<Lit> & conflict,int trail_pos=-1){
		//exit(8);
		if (trail_pos<0){
			trail_pos=trail.size();
		}
		rewind_trail_pos(trail_pos);
		stats_build_addition_arg_reason++;
		Weight  over_cur = over_approx[bvID];
		Weight  under_cur = under_approx[bvID];
		assert(checkApproxUpToDate(bvID));


		//the reason that the addition is over is the reason that
		//bvID > addition_under, or the reason that addition_under>= its current value.
		int other_argID = addition_arguments[bvID][argindex].other_argID;
		int sumID = addition_arguments[bvID][argindex].sumID;


		Weight under_add = under_approx[sumID] -  over_approx[other_argID];
		Weight over_add = over_approx[sumID] -  under_approx[other_argID];

		int width = bitvectors[bvID].size();
		Weight max_val = ((1L)<<width)-1;
		if(under_add>max_val){
			under_add=max_val;
		}
		if(over_add>max_val){
			over_add=max_val;
		}

		if(under_cur>over_add){
			//buildTrivialClause(conflict);
			buildValueReason(Comparison::leq, sumID,over_approx[sumID],conflict,trail_pos-1);

			buildValueReason(Comparison::geq, other_argID,under_approx[other_argID],conflict,trail_pos-1);
			buildValueReason(Comparison::gt, bvID,over_add,conflict,trail_pos-1);
		}else{
			assert(over_cur<under_add);
			//buildTrivialClause(conflict);
			buildValueReason(Comparison::geq, sumID,under_approx[sumID],conflict,trail_pos-1);
			buildValueReason(Comparison::leq, other_argID,over_approx[other_argID],conflict,trail_pos-1);
			buildValueReason(Comparison::lt, bvID,under_add,conflict,trail_pos-1);

		}

	}

	void buildAdditionReason(int bvID, vec<Lit> & conflict,int trail_pos=-1){
		//exit(8);
		if (trail_pos<0){
			trail_pos=trail.size();
		}
		rewind_trail_pos(trail_pos);
		stats_build_addition_reason++;
		Weight  over_cur = over_approx[bvID];
		Weight  under_cur = under_approx[bvID];
		assert(checkApproxUpToDate(bvID));
		assert(additions[bvID].hasAddition());

		//the reason that the addition is over is the reason that
		//bvID > addition_under, or the reason that addition_under>= its current value.
		int aID = additions[bvID].aID;
		int bID = additions[bvID].bID;

		assert(aID<bvID);
		assert(bID<bvID);

		Weight under_add = under_approx[aID] +  under_approx[bID];
		Weight over_add = over_approx[aID] +  over_approx[bID];

		int width = bitvectors[bvID].size();
		Weight max_val = ((1L)<<width)-1;
		if(under_add>max_val){
			under_add=max_val;
		}
		if(over_add>max_val){
			over_add=max_val;
		}

		if(under_cur>over_add){

			buildValueReason(Comparison::gt, bvID,over_add,conflict,trail_pos-1);

			buildValueReason(Comparison::leq, aID,over_approx[aID],conflict,trail_pos-1);
			buildValueReason(Comparison::leq, bID,over_approx[bID],conflict,trail_pos-1);
		}else{
			assert(over_cur<under_add);
			buildValueReason(Comparison::lt, bvID,under_add,conflict,trail_pos-1);

			buildValueReason(Comparison::geq, aID,under_approx[aID],conflict,trail_pos-1);
			buildValueReason(Comparison::geq, bID,under_approx[bID],conflict,trail_pos-1);
		}
	}

	void buildValueReasonBV(Comparison op, int bvID,int comparebvID, vec<Lit> & conflict,int trail_pos=-1){
		//exit(8);
		if (trail_pos<0){
			trail_pos=trail.size();
		}
		rewind_trail_pos(trail_pos);
		stats_build_value_bv_reason++;
		Weight  over_cur = over_approx[bvID];
		Weight  under_cur = under_approx[bvID];


		Weight  over_comp = over_approx[comparebvID];
		Weight  under_comp = under_approx[comparebvID];

		if (op==Comparison::lt){
			assert(over_cur<under_comp);

		}
		else if (op==Comparison::leq){
			assert(over_cur<=under_comp);

		}
		else if (op==Comparison::geq){
			assert(under_cur>=over_comp);

		}
		else if (op==Comparison::gt){
			assert(under_cur>over_comp);
		}

		if(isConst(bvID) && isConst(comparebvID)){
			//both bitvectors are constants, so no reason is required
			return;
		}else if (isConst(comparebvID)){
			assert(under_comp==over_comp);
			//if the other bitvector is a constant,
			//then the reason for the assignment is because this bitvector is (<,<=,>,>=) than the underapproximation of that constant.
			//newComparison(op,bvID,under_comp);
			//int cID = getComparisonID(op,bvID,under_comp);
			buildValueReason(op,bvID,under_comp,conflict,trail_pos-1);
		}else if (isConst(bvID)){
			assert(under_cur==over_cur);
			//if the other bitvector is a constant,
			//then the reason for the assignment is because this bitvector is (<,<=,>,>=) than the underapproximation of that constant.
			op=~op;
			//newComparison(op,comparebvID,over_cur);
			//int cID = getComparisonID(op,comparebvID,over_cur);
			buildValueReason(op,comparebvID,over_cur,conflict,trail_pos-1);
		}else{
			//neither bitvector is constant. Pick a value between them, and learn relative to that.
			Weight midval;
			Comparison cOp;

			if (op==Comparison::lt){
				assert(over_cur>=0);
				assert(under_comp>=0);
				assert(over_cur<under_comp);

				midval = ceildiv( under_comp- over_cur,2)+over_cur; //integer ceiling division
				assert(midval<=under_comp);
				assert(over_cur<midval);
				cOp=Comparison::geq;
				assert(midval!=over_cur);//because we took the integer ceiling division
			}
			else if (op==Comparison::leq){
				assert(over_cur<=under_comp);
				Weight m = under_comp- over_cur;
				midval = ceildiv( under_comp- over_cur,2)+over_cur; //integer ceiling division
				assert(midval<=under_comp);
				assert(over_cur<=midval);
				cOp=Comparison::geq;
			}
			else if (op==Comparison::geq){
				assert(under_cur>=over_comp);
				midval = (under_cur- over_comp)/2 + over_comp;//integer floor division
				assert(midval>=over_comp);
				assert(under_cur>=midval);
				cOp=Comparison::leq;
			}
			else{
				assert((op==Comparison::gt));
				assert(under_cur>over_comp);
				midval = (under_cur- over_comp)/2 + over_comp;//integer floor division
				assert(midval>=over_comp);
				assert(under_cur>midval);
				cOp=Comparison::leq;
			}

			//newComparison(op,bvID,midval);
			//int cID = getComparisonID(op,bvID,midval);
			buildValueReason(op,bvID,midval,conflict,trail_pos-1);

			//newComparison(op,comparebvID,midval);
			//cID = getComparisonID(op,comparebvID,midval);
			buildValueReason(cOp,comparebvID,midval,conflict,trail_pos-1);
		}


	}

	void buildValueReason(Comparison op, int bvID, Weight  to,  vec<Lit> & conflict,int trail_pos=-2){
		if(isConst(bvID)){
			// a constant bitvector needs no reason
			return;
		}
		stats_build_value_reason++;
		static int iter = 0;
		++iter;
		//printf("reason %d: %d\n",iter,bvID);
		if(iter==11){
			int a=1;
		}
		if (trail_pos<-1){
			trail_pos=trail.size();
		}else if (trail_pos<0){
			trail_pos=0;
		}
		rewind_trail_pos(trail_pos);
		trail_pos = rewindUntil(bvID,op,to);



		bool compare_over;

		vec<Lit> & bv = bitvectors[bvID];
		Weight  over_cur = over_approx[bvID];
		Weight  under_cur = under_approx[bvID];
		//assert(checkApproxUpToDate(bvID));
		if (op==Comparison::lt){
			assert(over_cur<to);
			compare_over=true;
		}
		if (op==Comparison::leq){
			assert(over_cur<=to);
			compare_over=true;
		}
		if (op==Comparison::geq){
			assert(under_cur>=to);
			compare_over=false;
		}
		if (op==Comparison::gt){
			assert(under_cur>to);
			compare_over=false;
		}

		if(!compare_over && under_approx[bvID]<= getUnderApprox(bvID, true) ){
			//no reason necessary; this is the lowest possible value.
			return;
		}else if(compare_over && over_approx[bvID]>=getOverApprox(bvID, true)  ){
			//no reason necessary; this is the lowest possible value.
			return;
		}


		if(compare_over)
			assert(over_causes[bvID].hasCause());
		if(!compare_over)
			assert(under_causes[bvID].hasCause());

		if (compare_over &&  over_causes[bvID].refined_cause){
			//then the reason the underapprox is too large is because of the assignment to the bits

			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				if(value(bl)==l_True ){

					//can probably improve on this later...
					assert(value(~bl)==l_False);
					conflict.push(~bl);

				}
			}
			//keep looking for further causes!
		}else if (!compare_over  && under_causes[bvID].refined_cause){
			//then the reason the underapprox is too large is because of the assignment to the bits

			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				lbool val = value(bl);
				lbool dbgval = dbg_value(bl);
				if(value(bl)==l_False){

						assert(value(bl)==l_False);
						conflict.push(bl);

				}
			}
			//keep looking for further causes!
		}


		if (compare_over &&  over_causes[bvID].cause_is_bits){
			//then the reason the underapprox is too large is because of the assignment to the bits
			Weight over = over_approx[bvID];
			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				if(value(bl)==l_False ){
					Weight bit = 1<<i;
					if(comp(op,over+bit,to)&& level(var(bl))>0){
						//then we can skip this bit, because we would still have had a conflict even if it was assigned true.
						over+=bit;
					}else{
						assert(value(bl)==l_False);
						conflict.push(bl);
					}
				}
			}

				return;
		}else if (!compare_over  && under_causes[bvID].cause_is_bits){
			//then the reason the underapprox is too large is because of the assignment to the bits
			Weight under = under_approx[bvID];
			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				lbool val = value(bl);
				lbool dbgval = dbg_value(bl);
				if(value(bl)==l_True){
					Weight bit = 1<<i;
					if(comp(op,under-bit,to)  && level(var(bl))>0){
						//then we can skip this bit, because we would still have had a conflict even if it was assigned false.
						under-=bit;
					}else{
						assert(value(~bl)==l_False);
						conflict.push(~bl);
					}
				}
			}

				return;
		}


		if(additions[bvID].hasAddition()){
			int aID = additions[bvID].aID;
			int bID = additions[bvID].bID;
			assert(aID<bvID);
			assert(bID<bvID);

			if  (compare_over && over_causes[bvID].cause_is_addition){
				//then the reason is that aID is <= weight-under(bID), or bID <= weight-under(aID)
				buildValueReason(op,aID,to-over_approx[bID],conflict,trail_pos-1);
				buildValueReason(op,bID,to-over_approx[aID],conflict,trail_pos-1);
				return;
			}else if(!compare_over && under_causes[bvID].cause_is_addition){
				buildValueReason(op,aID,to-under_approx[bID],conflict,trail_pos-1);
				buildValueReason(op,bID,to-under_approx[aID],conflict,trail_pos-1);
				return;
			}
		}

		if (compare_over && over_causes[bvID].cause_is_addition_arg>=0){
			int argindex = over_causes[bvID].cause_is_addition_arg;
			int other_argID = addition_arguments[bvID][argindex].other_argID;
			int sumID = addition_arguments[bvID][argindex].sumID;

			buildValueReason(op,other_argID,to+over_approx[sumID],conflict,trail_pos-1);
			buildValueReason(op,sumID,to+over_approx[other_argID],conflict,trail_pos-1);
			return;
		}
		if (!compare_over && under_causes[bvID].cause_is_addition_arg>=0){
			int argindex = under_causes[bvID].cause_is_addition_arg;
			int other_argID = addition_arguments[bvID][argindex].other_argID;
			int sumID = addition_arguments[bvID][argindex].sumID;

			buildValueReason(op,other_argID,to+over_approx[sumID],conflict,trail_pos-1);
			buildValueReason(op,sumID,to+over_approx[other_argID],conflict,trail_pos-1);
			return;
		}
		int cID;
		if (compare_over){
			cID= over_causes[bvID].comparison_cause;
		}else{
			cID= under_causes[bvID].comparison_cause;
		}
		if(cID>=-1){
			ComparisonID & c = comparisons[cID];
			Comparison cop = c.op();//invert this because we are switch the direction of comparison



			if(value(c.l)==l_True){
				assert(value(~c.l)==l_False);
				conflict.push(~c.l);
				if(c.bvCompare()){
					if (compare_over){
						buildValueReason(Comparison::leq,c.compareID, over_approx[c.compareID],conflict,trail_pos-1);
					}else{
						buildValueReason(Comparison::geq,c.compareID, under_approx[c.compareID],conflict,trail_pos-1);
					}
				}
			}else{
				assert(value(c.l)==l_False);
				conflict.push(c.l);
				if(c.bvCompare()){
					if (compare_over){
						buildValueReason(Comparison::leq,c.compareID, over_approx[c.compareID],conflict, trail_pos-1);
					}else{
						buildValueReason(Comparison::geq,c.compareID, under_approx[c.compareID],conflict,trail_pos-1);
					}
				}
			}


			return;
		}
		assert(false);//no cause was found.
	}
/*
	void buildValueLTReason(int bvID, int comparisonID, vec<Lit> & conflict){
		printf("lt reason %d %d\n",bvID, comparisonID);
		Weight & lt = getComparison(comparisonID).w;
		Lit l =  getComparison(comparisonID).l;

		vec<Lit> & bv = bitvectors[bvID];
		Weight  over_cur = over_approx[bvID];
		assert(checkApproxUpToDate(bvID));
		assert(over_cur<lt);


		//the reason that the value is less than the weight 'lt' is that the _overapprox_ of the weight is less than lt.
		for(int i =0;i<bv.size();i++){
			Lit bl = bv[i];
			if(value(bl)==l_False && level(var(bl))>0){
				Weight bit = 1<<i;
				if(over+bit<lt){
					//then we can skip this bit, because we would still have had a conflict even if it was assigned true.
					over+=bit;
				}else{
					conflict.push(bl);
				}
			}
		}
		Weight under =0;
		Weight over=0;
		for(int i = 0;i<bv.size();i++){
			lbool val = value(bv[i]);
			if(val==l_True){
				Weight bit = 1<<i;
				under+=bit;
				over+=bit;
			}else if (val==l_False){

			}else{
				Weight bit = 1<<i;
				over+=bit;
			}
		}
		if (over<lt){
			//then the reason the underapprox is too large is because of the assignment to the bits
			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				if(value(bl)==l_False ){
					Weight bit = 1<<i;
					if(over+bit<lt&& level(var(bl))>0){
						//then we can skip this bit, because we would still have had a conflict even if it was assigned true.
						over+=bit;
					}else{
						assert(value(bl)==l_False);
						conflict.push(bl);
					}
				}
			}
			return;
		}

		for(int cID:comparisons_lt[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && over>=c.w){
				over=c.w-1;
			}else if (value(c.l)==l_False && under<c.w){
				under=c.w;
			}
			if (over<lt){
				//this is the reason for the bitvector being too large
				assert(value(~c.l)==l_False);
				conflict.push(~c.l);
				return;
			}
		}
		for(int cID:comparisons_leq[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && over>c.w){
				over=c.w;
			}else if (value(c.l)==l_False && under<=c.w){
				under=c.w+1;
			}
			if (over<lt){
				//this is the reason for the bitvector being too large
				assert(value(~c.l)==l_False);
				conflict.push(~c.l);
				return;
			}
		}
		for(int cID:comparisons_gt[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && under<=c.w){
				under=c.w+1;
			}else if (value(c.l)==l_False && over>c.w){
				over=c.w;
			}
			if (over<lt){
				//this is the reason for the bitvector being too large
				assert(value(c.l)==l_False);
				conflict.push(c.l);
				return;
			}
		}
		for(int cID:comparisons_geq[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && under<c.w){
				under=c.w;
			}else if (value(c.l)==l_False && over>=c.w){
				over=c.w-1;
			}
			if (over<lt){
				//this is the reason for the bitvector being too large
				assert(value(c.l)==l_False);
				conflict.push(c.l);
				return;
			}
		}
	}

	void buildValueGEQReason(int bvID, int comparisonID, vec<Lit> & conflict){
		printf("geq reason %d %d\n",bvID, comparisonID);
		Weight & lt = getComparison(comparisonID).w;
		Lit l =  getComparison(comparisonID).l;

		vec<Lit> & bv = bitvectors[bvID];
		Weight  under_cur = under_approx[bvID];
		assert(checkApproxUpToDate(bvID));
		assert(under_cur>=lt);
		//the reason that the value is less than the weight 'lt' is that the _overapprox_ of the weight is less than lt.
		//the reason for this depends on how the over approximation was computed - that is, whether it is caused by an assignment to a comparison or to the bitvector itself.

		//this analysis is _only_ correct if we have correctly backtracked in the trail to the point where the comparison was assigned.

		Weight under =0;
		Weight over=0;
		for(int i = 0;i<bv.size();i++){
			lbool val = value(bv[i]);
			if(val==l_True){
				Weight bit = 1<<i;
				under+=bit;
				over+=bit;
			}else if (val==l_False){

			}else{
				Weight bit = 1<<i;
				over+=bit;
			}
		}
		if (under>=lt){
			//then the reason the underapprox is too large is because of the assignment to the bits
			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				lbool val = value(bl);
				lbool dbgval = dbg_value(bl);
				if(value(bl)==l_True){
					Weight bit = 1<<i;
					if(under-bit>=lt  && level(var(bl))>0){
						//then we can skip this bit, because we would still have had a conflict even if it was assigned false.
						under-=bit;
					}else{
						assert(value(~bl)==l_False);
						conflict.push(~bl);
					}
				}
			}
			return;
		}

		for(int cID:comparisons_lt[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && over>=c.w){
				over=c.w-1;
			}else if (value(c.l)==l_False && under<c.w){
				under=c.w;
			}
			if (under>=lt){
				//this is the reason for the bitvector being too large
				assert(value(c.l)==l_False);
				conflict.push(c.l);
				return;
			}
		}
		for(int cID:comparisons_leq[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && over>c.w){
				over=c.w;
			}else if (value(c.l)==l_False && under<=c.w){
				under=c.w+1;
			}
			if (under>=lt){
				//this is the reason for the bitvector being too large
				assert(value(c.l)==l_False);
				conflict.push(c.l);
				return;
			}
		}
		for(int cID:comparisons_gt[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && under<=c.w){
				under=c.w+1;
			}else if (value(c.l)==l_False && over>c.w){
				over=c.w;
			}
			if (under>=lt){
				//this is the reason for the bitvector being too large
				assert(value(~c.l)==l_False);
				conflict.push(~c.l);
				return;
			}
		}
		for(int cID:comparisons_geq[bvID]){
			if(cID == comparisonID)
				continue;
			ComparisonID & c = comparisons[cID];
			if(value( c.l)==l_True && under<c.w){
				under=c.w;
			}else if (value(c.l)==l_False && over>=c.w){
				over=c.w-1;
			}
			if (under>=lt){
				//this is the reason for the bitvector being too large
				assert(value(~c.l)==l_False);
				conflict.push(~c.l);
				return;
			}
		}


	}

	void buildValueLEQReason(int bvID, int comparisonID, vec<Lit> & conflict){
			printf("leq reason %d %d\n",bvID, comparisonID);
			Weight & lt = getComparison(comparisonID).w;
			Lit l =  getComparison(comparisonID).l;

			vec<Lit> & bv = bitvectors[bvID];
			Weight  over_cur = over_approx[bvID];
			assert(checkApproxUpToDate(bvID));
			assert(over_cur<=lt);


			Weight under =0;
			Weight over=0;
			for(int i = 0;i<bv.size();i++){
				lbool val = value(bv[i]);
				if(val==l_True){
					Weight bit = 1<<i;
					under+=bit;
					over+=bit;
				}else if (val==l_False){

				}else{
					Weight bit = 1<<i;
					over+=bit;
				}
			}
			if (over<=lt){
				//then the reason the underapprox is too large is because of the assignment to the bits
				for(int i =0;i<bv.size();i++){
					Lit bl = bv[i];
					if(value(bl)==l_False ){
						Weight bit = 1<<i;
						if(over+bit<=lt&& level(var(bl))>0){
							//then we can skip this bit, because we would still have had a conflict even if it was assigned true.
							over+=bit;
						}else{
							assert(value(bl)==l_False);
							conflict.push(bl);
						}
					}
				}
				return;
			}

			for(int cID:comparisons_lt[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && over>=c.w){
					over=c.w-1;
				}else if (value(c.l)==l_False && under<c.w){
					under=c.w;
				}
				if (over<=lt){
					//this is the reason for the bitvector being too large
					assert(value(~c.l)==l_False);
					conflict.push(~c.l);
					return;
				}
			}
			for(int cID:comparisons_leq[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && over>c.w){
					over=c.w;
				}else if (value(c.l)==l_False && under<=c.w){
					under=c.w+1;
				}
				if (over<=lt){
					//this is the reason for the bitvector being too large
					assert(value(~c.l)==l_False);
					conflict.push(~c.l);
					return;
				}
			}
			for(int cID:comparisons_gt[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && under<=c.w){
					under=c.w+1;
				}else if (value(c.l)==l_False && over>c.w){
					over=c.w;
				}
				if (over<=lt){
					//this is the reason for the bitvector being too large
					assert(value(c.l)==l_False);
					conflict.push(c.l);
					return;
				}
			}
			for(int cID:comparisons_geq[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && under<c.w){
					under=c.w;
				}else if (value(c.l)==l_False && over>=c.w){
					over=c.w-1;
				}
				if (over<=lt){
					//this is the reason for the bitvector being too large
					assert(value(c.l)==l_False);
					conflict.push(c.l);
					return;
				}
			}
		}

		void buildValueGTReason(int bvID, int comparisonID, vec<Lit> & conflict){
			printf("gt reason %d %d\n",bvID, comparisonID);
			Weight & lt = getComparison(comparisonID).w;
			Lit l =  getComparison(comparisonID).l;

			vec<Lit> & bv = bitvectors[bvID];
			Weight  under_cur = under_approx[bvID];
			assert(checkApproxUpToDate(bvID));
			assert(under_cur>lt);
			//the reason that the value is less than the weight 'lt' is that the _overapprox_ of the weight is less than lt.

			for(int i =0;i<bv.size();i++){
				Lit bl = bv[i];
				if(value(bl)==l_True && level(var(bl))>0){
					Weight bit = 1<<i;
					if(under-bit>lt){
						//then we can skip this bit, because we would still have had a conflict even if it was assigned false.
						under-=bit;
					}else{
						conflict.push(~bl);
					}
				}
			}

			Weight under =0;
			Weight over=0;
			for(int i = 0;i<bv.size();i++){
				lbool val = value(bv[i]);
				if(val==l_True){
					Weight bit = 1<<i;
					under+=bit;
					over+=bit;
				}else if (val==l_False){

				}else{
					Weight bit = 1<<i;
					over+=bit;
				}
			}
			if (under>lt){
				//then the reason the underapprox is too large is because of the assignment to the bits
				for(int i =0;i<bv.size();i++){
					Lit bl = bv[i];
					lbool val = value(bl);
					lbool dbgval = dbg_value(bl);
					if(value(bl)==l_True){
						Weight bit = 1<<i;
						if(under-bit>lt  && level(var(bl))>0){
							//then we can skip this bit, because we would still have had a conflict even if it was assigned false.
							under-=bit;
						}else{
							assert(value(~bl)==l_False);
							conflict.push(~bl);
						}
					}
				}
				return;
			}

			for(int cID:comparisons_lt[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && over>=c.w){
					over=c.w-1;
				}else if (value(c.l)==l_False && under<c.w){
					under=c.w;
				}
				if (under>lt){
					//this is the reason for the bitvector being too large
					assert(value(c.l)==l_False);
					conflict.push(c.l);
					return;
				}
			}
			for(int cID:comparisons_leq[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && over>c.w){
					over=c.w;
				}else if (value(c.l)==l_False && under<=c.w){
					under=c.w+1;
				}
				if (under>lt){
					//this is the reason for the bitvector being too large
					assert(value(c.l)==l_False);
					conflict.push(c.l);
					return;
				}
			}
			for(int cID:comparisons_gt[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && under<=c.w){
					under=c.w+1;
				}else if (value(c.l)==l_False && over>c.w){
					over=c.w;
				}
				if (under>lt){
					//this is the reason for the bitvector being too large
					assert(value(~c.l)==l_False);
					conflict.push(~c.l);
					return;
				}
			}
			for(int cID:comparisons_geq[bvID]){
				if(cID == comparisonID)
					continue;
				ComparisonID & c = comparisons[cID];
				if(value( c.l)==l_True && under<c.w){
					under=c.w;
				}else if (value(c.l)==l_False && over>=c.w){
					over=c.w-1;
				}
				if (under>lt){
					//this is the reason for the bitvector being too large
					assert(value(~c.l)==l_False);
					conflict.push(~c.l);
					return;
				}
			}
		}*/

	bool solveTheory(vec<Lit> & conflict) {
		requiresPropagation = true;		//Just to be on the safe side... but this shouldn't really be required.
		bool ret = propagateTheory(conflict);
		//Under normal conditions, this should _always_ hold (as propagateTheory should have been called and checked by the parent solver before getting to this point).
		assert(ret);
		return ret;
	}

	bool check_solved() {
		for(int bvID = 0;bvID<bitvectors.size();bvID++){
			vec<Lit> & bv = bitvectors[bvID];
			Weight over=0;
			Weight under=0;
			for(int i = 0;i<bv.size();i++){
				lbool val = value(bv[i]);
				if(val==l_True){
					Weight bit = 1<<i;
					under+=bit;
					over+=bit;
				}else if (val==l_False){

				}else{
					Weight bit = 1<<i;
					over+=bit;
				}
			}

			if(additions[bvID].hasAddition()){
				int aID = additions[bvID].aID;
				int bID = additions[bvID].bID;
				int width = bitvectors[bvID].size();
				Weight max_val = (1L<<width)-1;
				assert(aID<bvID);
				assert(bID<bvID);
				Weight underadd = under_approx[aID] +  under_approx[bID];
				Weight overadd = over_approx[aID] +  over_approx[bID];
				if(underadd>max_val){
					underadd=max_val;
				}
				if(overadd>max_val){
					overadd=max_val;
				}
				if(underadd >under){
					return false;
				}
				if(overadd<over){
					return false;
				}
			}

			for(int cID:compares[bvID]){
				ComparisonID & c = comparisons[cID];
				assert(c.compareID<0);
				Comparison op = c.op();
				switch (op){
					case Comparison::lt:
						if(value(c.l)==l_True && under>= c.w){
							return false;
						}else if (value(c.l)==l_False && over<c.w){
							return false;
						}
						break;
					case Comparison::leq:
						if(value(c.l)==l_True && under> c.w){
							return false;
						}else if (value(c.l)==l_False && over<=c.w){
							return false;
						}
						break;
					case Comparison::gt:
						if(value(c.l)==l_True && over<= c.w){
							return false;
						}else if (value(c.l)==l_False && under>c.w){
							return false;
						}
						break;
					case Comparison::geq:
					default:
						if(value(c.l)==l_True && over < c.w){
							return false;
						}else if (value(c.l)==l_False && under>= c.w){
							return false;
						}
						break;
				}

			}

			for(int cID:bvcompares[bvID]){
				ComparisonID & c = comparisons[cID];
				assert(c.compareID>=0);
				Comparison op = c.op();
				int toID = c.compareID;
				vec<Lit> & bv_compare = bitvectors[toID];
				Weight over_compare=0;
				Weight under_compare=0;
				for(int i = 0;i<bv_compare.size();i++){
					lbool val = value(bv_compare[i]);
					if(val==l_True){
						Weight bit = 1<<i;
						under_compare+=bit;
						over_compare+=bit;
					}else if (val==l_False){

					}else{
						Weight bit = 1<<i;
						over_compare+=bit;
					}
				}

				switch (op){
					case Comparison::lt:
						if(value(c.l)==l_True && under>= over_compare){
							return false;
						}else if (value(c.l)==l_False && over<under_compare){
							return false;
						}
						break;
					case Comparison::leq:
						if(value(c.l)==l_True && under> over_compare){
							return false;
						}else if (value(c.l)==l_False && over<=under_compare){
							return false;
						}
						break;
					case Comparison::gt:
						if(value(c.l)==l_True && over<= under_compare){
							return false;
						}else if (value(c.l)==l_False && under>over_compare){
							return false;
						}
						break;
					case Comparison::geq:
					default:
						if(value(c.l)==l_True && over < under_compare){
							return false;
						}else if (value(c.l)==l_False && under>= over_compare){
							return false;
						}
						break;
				}
			}
		}
		return true;
	}
	
	bool dbg_solved() {

		return true;
	}




	void setBitvectorTheory(int bvID, int theoryID){
		theoryIds[bvID]=theoryID;
	}


	BitVector newAdditionBV(int resultID, int aID, int bID){
		int bitwidth = getBV(resultID).width();
		if(resultID<=aID || resultID<=bID){
			std::cerr<<"Addition result must have a strictly greater id than its arguments\n";
			exit(1);
		}
		if(bitwidth !=  getBV(aID).width()){
			std::cerr<<"Bit widths must match for bitvectors\n";
			exit(1);
		}
		if(bitwidth !=  getBV(bID).width()){
			std::cerr<<"Bit widths must match for bitvectors\n";
			exit(1);
		}

		if(additions[resultID].aID>-1){
			std::cerr<<"Bitvectors can be the sum of at most one addition\n";
			exit(1);
		}

		additions[resultID].aID=aID;
		additions[resultID].bID=bID;
		addition_arguments[aID].push();
		addition_arguments[aID].last().other_argID=bID;
		addition_arguments[aID].last().sumID=resultID;

		addition_arguments[bID].push();
		addition_arguments[bID].last().other_argID=aID;
		addition_arguments[bID].last().sumID=resultID;
		//addition_arguments[bID].push({aID,resultID});
		if(!cause_set[aID].contains(resultID))
			cause_set[aID].push(resultID);
		if(!cause_set[bID].contains(resultID))
			cause_set[bID].push(resultID);
		//additions[resultID].w=0;
		return getBV(resultID);
	}

	BitVector newBitvector(int bvID, vec<Var> & vars){
		if(bvID<0){
			bvID = bitvectors.size();
		}
		n_bits+=vars.size();
		//bv_callbacks.growTo(id+1,nullptr);
		bitvectors.growTo(bvID+1);
		theoryIds.growTo(bvID+1,-1);

		under_approx.growTo(bvID+1,-1);
		over_approx.growTo(bvID+1,-1);
		under_approx0.growTo(bvID+1,-1);
		over_approx0.growTo(bvID+1,-1);
		alteredBV.growTo(bvID+1);
		bvcompares.growTo(bvID+1);
		compares.growTo(bvID+1);
		bvconst.growTo(bvID+1);
		additions.growTo(bvID+1);
		addition_arguments.growTo(bvID+1);
		under_causes.growTo(bvID+1);
		over_causes.growTo(bvID+1);
		cause_set.growTo(bvID+1);
		//bv_callbacks.growTo(bvID+1);
		if(under_approx[bvID]>-1){
			assert(false);
			fprintf(stderr,"Redefined bitvector, Aborting!");
			exit(1);
		}
		under_approx[bvID]=0;
		over_approx[bvID]=(1L<<vars.size())-1;
		under_approx0[bvID]=under_approx[bvID];
		over_approx0[bvID]=over_approx[bvID];


		for(int i = 0;i<vars.size();i++){
			bitvectors[bvID].push(mkLit(newVar(vars[i],bvID)));
		}

		alteredBV[bvID]=true;
		altered_bvs.push(bvID);
		return BitVector(*this,bvID);
	}

	void makeConst(int bvID, Weight c){
		BitVector bv = getBV(bvID);
		for (int i = bv.width()-1;i>=0;i--){
			Weight v = ((Weight)1)<<i;
			Lit l = bv.getBits()[i];
			if(c>=v){
				c-=v;

				addClause(l);
			}else{
				addClause(~l);
			}
		}
	}

	BitVector newBitvector(int bvID, int bitwidth){
		if(bvID<0){
			bvID = bitvectors.size();
		}
		n_bits+=bitwidth;
		//bv_callbacks.growTo(id+1,nullptr);
		bitvectors.growTo(bvID+1);
		theoryIds.growTo(bvID+1,-1);

		under_approx.growTo(bvID+1,-1);
		over_approx.growTo(bvID+1,-1);
		under_approx0.growTo(bvID+1,-1);
		over_approx0.growTo(bvID+1,-1);
		bvconst.growTo(bvID+1);
		alteredBV.growTo(bvID+1);
		bvcompares.growTo(bvID+1);
		compares.growTo(bvID+1);
		additions.growTo(bvID+1);
		addition_arguments.growTo(bvID+1);
		under_causes.growTo(bvID+1);
		over_causes.growTo(bvID+1);
		cause_set.growTo(bvID+1);
		//bv_callbacks.growTo(bvID+1);
		if(under_approx[bvID]>-1){
			assert(false);
			fprintf(stderr,"Redefined bitvector, Aborting!");
			exit(1);
		}
		under_approx[bvID]=0;
		over_approx[bvID]=(1L<<vars.size())-1;
		under_approx0[bvID]=under_approx[bvID];
		over_approx0[bvID]=over_approx[bvID];
		for(int i = 0;i<bitwidth;i++){
			bitvectors[bvID].push(mkLit(newVar(var_Undef,bvID)));
		}

		alteredBV[bvID]=true;
		altered_bvs.push(bvID);
		return BitVector(*this,bvID);
	}

	bool isConst(int bvID)const{
		return bvconst[bvID];
	}

	bool hasBV(int bvID){
		return bvID>=0 && bvID<under_approx.size() && under_approx[bvID]>-1;
	}
private:
	Lit getComparison(Comparison op, int bvID,const Weight & lt){
		//could do a binary search here:
		for(int i=0;i<compares[bvID].size();i++){
			int cID = compares[bvID][i];
			if (comparisons[cID].op() == op && comparisons[cID].w == lt){
				return comparisons[cID].l;
			}
		}

		return lit_Undef;
	}
	int getComparisonID(Comparison op, int bvID,const Weight & lt){
		//could do a binary search here:
		for(int i=0;i<compares[bvID].size();i++){
			int cID = compares[bvID][i];
			if (comparisons[cID].op() == op && comparisons[cID].w == lt){
				return cID;
			}
		}

		return -1;
	}
	Lit getComparisonBV(Comparison op, int bvID,int compareID){

		if(bvID>compareID){
			Lit c =getComparisonBV(-op,compareID,bvID);
			if(c!=lit_Undef){
				return ~c;
			}else{
				return lit_Undef;
			}
		}

		vec<int> & bvcompare = bvcompares[bvID];
		for(int i=0;i<bvcompare.size();i++){
			int cID = bvcompare[i];
			if (comparisons[cID].compareID == compareID && comparisons[cID].op()==op){
				return comparisons[cID].l;
			}
		}
		return lit_Undef;
	}

public:
	Lit newComparison(Comparison op, int bvID,const Weight & to, Var outerVar = var_Undef) {
		Lit l;
		if(!hasBV(bvID)){
			exit(1);
		}
		int comparisonID = comparisons.size();
		if(comparisonID==8){
			int a=1;
		}
		if((l = getComparison(op, bvID, to))!=lit_Undef){
			if(outerVar != var_Undef){
				makeEqualInSolver(mkLit(outerVar),toSolver(l));
			}
			return l;
		}else{
			l = mkLit(newVar(outerVar, bvID,comparisonID));
		}

#ifndef NDEBUG
		std::cout << "learnt fact " << "bv " << bvID << " " << op << " " << to  <<" " << dimacs(toSolver(l)) << "\n";
#endif

		updateApproximations(bvID);
		comparisons.push(ComparisonID(to,-1,l,bvID,op));
		compares[bvID].push(comparisonID);
		//insert this value in order.
		//could do a binary search here...
		for(int i=0;i<compares[bvID].size()-1;i++){
			int cid = compares[bvID][i];
			if(comparisons[cid].w>= to){
				for(int j = compares[bvID].size()-1; j>i ;j--){
					compares[bvID][j]=compares[bvID][j-1];
				}
				compares[bvID][i]=comparisonID;
				break;
			}
		}
		if(!alteredBV[bvID]){
			alteredBV[bvID]=true;
			altered_bvs.push(bvID);
		}
		//set the value of this immediately, if needed

		Weight & underApprox = under_approx[bvID];
		Weight & overApprox = over_approx[bvID];
		Cause  prev_under_cause = under_causes[bvID];
		Cause  prev_over_cause = over_causes[bvID];
		switch (op){
			case Comparison::lt:
				if (overApprox<to){
					if(value(l)==l_True){
						//do nothing
					}else if (value(l)==l_False){
						assert(false);//this should not happen!
					}else {
						assert(value(l)==l_Undef);

						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>=to){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){
						//do nothing
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::leq:
				if (overApprox<=to){
					if(value(l)==l_True){
						//do nothing
					}else if (value(l)==l_False){
						assert(false);//this should not happen!
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>to){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){
						//do nothing
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::gt:
				if (overApprox<=to){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){

					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>to){
					if(value(l)==l_True){

					}else if (value(l)==l_False){
						assert(false);
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::geq:
			default:
				if (overApprox<to){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){

					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>=to){
					if(value(l)==l_True){

					}else if (value(l)==l_False){
						assert(false);
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
		}
		return l;
	}



	Lit newComparisonBV(Comparison op, int bvID,int toID, Var outerVar = var_Undef) {

		if(bvID<toID){

			Lit l = newComparisonBV(~op,toID,bvID,outerVar);//is this correct?
		/*	if(outerVar !=var_Undef){
				makeEqualInSolver(mkLit(outerVar),toSolver(~l));
			}*/
			return l;
		}
		if(!hasBV(bvID)){
			printf("PARSE ERROR! Undefined bitvector %d\n", bvID), exit(1);
		}
		if(!hasBV(toID)){
			printf("PARSE ERROR! Undefined bitvector %d\n", toID), exit(1);
		}
		Lit l;
		int comparisonID = comparisons.size();
		if((l = getComparisonBV(op,bvID, toID))!=lit_Undef){
			if(outerVar != var_Undef){
				makeEqualInSolver(mkLit(outerVar),toSolver(l));
			}
			return l;
		}else{
			l = mkLit(newVar(outerVar, bvID,comparisonID));
		}


		updateApproximations(bvID);
		updateApproximations(toID);

		if(!cause_set[toID].contains(bvID))
			cause_set[toID].push(bvID);

		comparisons.push(ComparisonID(-1,toID,l,bvID,op));
		bvcompares[bvID].push(comparisonID);
		//insert this value in order.
		//could do a binary search here...
		for(int i=0;i<bvcompares[bvID].size()-1;i++){
			int cid = bvcompares[bvID][i];
			if(comparisons[cid].compareID>= toID){
				for(int j = bvcompares[bvID].size()-1; j>i ;j--){
					bvcompares[bvID][j]=bvcompares[bvID][j-1];
				}
				bvcompares[bvID][i]=comparisonID;
				break;
			}
		}

		if(!cause_set[bvID].contains(toID))
			cause_set[bvID].push(toID);
		//Also need to attach an equivalent (but reversed) comparator to the other bitvector
		comparisonID = comparisons.size();
		comparisons.push(ComparisonID(-1,bvID,l,toID,~op));
		bvcompares[toID].push(comparisonID);
		//insert this value in order.
		//could do a binary search here...
		for(int i=0;i<bvcompares[toID].size()-1;i++){
			int cid = bvcompares[toID][i];
			if(comparisons[cid].compareID>= bvID){
				for(int j = bvcompares[toID].size()-1; j>i ;j--){
					bvcompares[toID][j]=bvcompares[toID][j-1];
				}
				bvcompares[toID][i]=comparisonID;
				break;
			}
		}

		if(!alteredBV[bvID]){
			alteredBV[bvID]=true;
			altered_bvs.push(bvID);
		}
		if(!alteredBV[toID]){
			alteredBV[toID]=true;
			altered_bvs.push(toID);
		}

		Weight & underApprox = under_approx[bvID];
		Weight & overApprox = over_approx[bvID];

		Weight & underCompare = under_approx[toID];
		Weight & overCompare = over_approx[toID];
		Cause  prev_under_cause = under_causes[bvID];
		Cause  prev_over_cause = over_causes[bvID];
		switch (op){
			case Comparison::lt:
				if (overApprox<underCompare){
					if(value(l)==l_True){
						//do nothing
					}else if (value(l)==l_False){
						assert(false);//this should not happen!
					}else {
						assert(value(l)==l_Undef);

						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>=overCompare){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){
						//do nothing
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::leq:
				if (overApprox<=underCompare){
					if(value(l)==l_True){
						//do nothing
					}else if (value(l)==l_False){
						assert(false);//this should not happen!
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>overCompare){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){
						//do nothing
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::gt:
				if (overApprox<=underCompare){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){

					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>overCompare){
					if(value(l)==l_True){

					}else if (value(l)==l_False){
						assert(false);
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
			case Comparison::geq:
			default:
				if (overApprox<underCompare){
					if(value(l)==l_True){
						assert(false);
					}else if (value(l)==l_False){

					}else {
						assert(value(l)==l_Undef);
						enqueueEager(~l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}

				if (underApprox>=overCompare){
					if(value(l)==l_True){

					}else if (value(l)==l_False){
						assert(false);
					}else {
						assert(value(l)==l_Undef);
						enqueueEager(l,bvID,underApprox,overApprox,prev_under_cause,prev_over_cause, comparisonprop_marker);
					}
				}
				break;
		}

		return l;
	}

	void printSolution() {

	}

	bool dbg_uptodate(){
#ifndef NDEBUG
		//dbg_synced();
		for(int bvID = 0;bvID<bitvectors.size();bvID++){
			Weight under;
			Weight over;
			assert(checkApproxUpToDate(bvID,&under,&over));

			//Weight & under = under_approx[bvID];
			//Weight & over = over_approx[bvID];
				for(int cID:compares[bvID]){
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			lbool val = value(c.l);
			if(!c.bvCompare()){
				switch(op){
					case Comparison::lt:
						if(over<c.w){
							assert(value(c.l)==l_True);
						}else if (under>=c.w){
							assert(value(c.l)==l_False);
						}

						break;
					case Comparison::leq:
						if(over<=c.w){
							assert(value(c.l)==l_True);
						}else if (under>c.w){
							assert(value(c.l)==l_False);
						}
						break;
					case Comparison::gt:
						if(over<=c.w){
							assert(value(c.l)==l_False);
						}else if (under>c.w){
							assert(value(c.l)==l_True);
						}
						break;
					case Comparison::geq:
					default:
						if(over<c.w){
							assert(value(c.l)==l_False);
						}else if (under>=c.w){
							assert(value(c.l)==l_True);
						}
					break;
				}
			}else{


			}
		}
		for(int cID:bvcompares[bvID]){
			ComparisonID & c = comparisons[cID];
			Comparison op = c.op();

			lbool val = value(c.l);
			if(c.bvCompare()){
				Weight under_w = under_approx[c.compareID];
				Weight over_w = over_approx[c.compareID];

				switch(op){

					case Comparison::lt:
						if( over<under_w){
							assert(value(c.l)==l_True);
						}else if (under>=over_w){
							assert(value(c.l)==l_False);
						}
						break;
					case Comparison::leq:
						if( over<=under_w){
							assert(value(c.l)==l_True);
						}else if (under>over_w){
							assert(value(c.l)==l_False);
						}
						break;
					case Comparison::gt:
						if( over<=under_w){
							assert(value(c.l)==l_False);
						}else if (under>over_w){
							assert(value(c.l)==l_True);
						}
						break;
					case Comparison::geq:
					default:
						if( over<under_w){
							assert(value(c.l)==l_False);
						}else if (under>=over_w){
							assert(value(c.l)==l_True);
						}
						break;
				}
			}else{


			}
		}
		}
#endif
		return true;
	}

};


template<typename Weight>
inline Weight BVTheorySolver<Weight>::ceildiv(Weight a, Weight b){
	Weight rem = a%b;
	Weight div = a/b;
	return  (rem !=0) ? (div+1) : div;
}
template<>
inline mpq_class BVTheorySolver<mpq_class>::ceildiv(mpq_class a, mpq_class b){
	//what is the correct way to do this, if any?
	assert(false);//this is not properly implemented yet...
	return a/b;

}
template<>
inline mpf_class BVTheorySolver<mpf_class>::ceildiv(mpf_class a, mpf_class b){
	return ceil(a/b);
}
template<>
inline double BVTheorySolver<double>::ceildiv(double a, double b){
	return std::ceil(a/b);
}

template<>
inline float BVTheorySolver<float>::ceildiv(float a, float b){
	return std::ceil(a/b);
}
template<>
inline mpq_class BVTheorySolver<mpq_class>::lowest(int bvID){
	exit(1);
}
template<>
inline mpq_class BVTheorySolver<mpq_class>::highest(int bvID){
	exit(1);
}
template<>
inline mpq_class BVTheorySolver<mpq_class>::refine_ubound(int bvID, mpq_class bound, Var ignore_bit){
	exit(1);
}
template<>
inline double BVTheorySolver<double>::lowest(int bvID){
	exit(1);
}
template<>
inline double BVTheorySolver<double>::highest(int bvID){
	exit(1);
}
template<>
inline double BVTheorySolver<double>::refine_ubound(int bvID, double bound, Var ignore_bit){
	exit(1);
}
template<>
inline mpq_class BVTheorySolver<mpq_class>::refine_lbound(int bvID, mpq_class bound, Var ignore_bit){
	exit(1);
}
template<>
inline double BVTheorySolver<double>::refine_lbound(int bvID, double bound, Var ignore_bit){
	exit(1);
}
template<typename Weight>
Weight BVTheorySolver<Weight>::lowest(int bvID){
	    Weight val= 0;
	    vec<Lit> & bv = bitvectors[bvID];
	    for (int i = 0;i<bv.size();i++){
	        if (value(bv[i])==l_True)
	            val += ((Weight )1)<<i;
	    }
	    return val;
	}
template<typename Weight>
	Weight BVTheorySolver<Weight>::highest(int bvID){
	    Weight val= 0;
	    vec<Lit> & bv = bitvectors[bvID];
	    for (int i = 0;i<bv.size();i++){
	        if (value(bv[i])!=l_False)
	            val += ((Weight )1)<<i;
	    }
	    return val;
	}

template<>
inline mpq_class BVTheorySolver<mpq_class>::refine_ubound_check(int bvID, mpq_class bound, Var ignore_bit){
}
template<>
inline double BVTheorySolver<double>::refine_ubound_check(int bvID, double bound, Var ignore_bit){
}


template<>
inline mpq_class BVTheorySolver<mpq_class>::refine_lbound_check(int bvID, mpq_class bound, Var ignore_bit){
}
template<>
inline double BVTheorySolver<double>::refine_lbound_check(int bvID, double bound, Var ignore_bit){
}

template<>
inline void BVTheorySolver<double>::dbg_evaluate(int bvID, int pos,vec<double> & vals,double val){

}
template<>
inline void BVTheorySolver<mpq_class>::dbg_evaluate(int bvID, int pos,vec<mpq_class> & vals,mpq_class val){

}

template<typename Weight>
void BVTheorySolver<Weight>::dbg_evaluate(int bvID, int pos,vec<Weight> & vals,Weight val){
    if(pos==-1){
        vals.push(val);
        return;
    }
    vec<Lit> & bv = bitvectors[bvID];
    if (value(bv[pos]) ==l_True or value(bv[pos]) ==l_Undef){
    	Weight bit = (1<<pos);
		dbg_evaluate(bvID,pos-1,vals, val+bit);
    }
    if (value(bv[pos]) ==l_False or value(bv[pos]) ==l_Undef)
    	dbg_evaluate(bvID,pos-1,vals, val);
}
template<typename Weight>
Weight BVTheorySolver<Weight>::refine_ubound_check(int bvID, Weight bound, Var ignore_bit){
#ifndef NDEBUG
	//test all values of mbits, find the lowest one >= i
	static vec<Weight> vals;
	vals.clear();

    dbg_evaluate(bvID,bitvectors[bvID].size()-1,vals,0);
    Weight lowest_bound = -1;
    for(Weight x:vals){
    	if (x>= bound && (lowest_bound<0 || x<lowest_bound)){
    		lowest_bound=x;
    	}
    }
    return lowest_bound;

#endif
    return 0;
}

template<typename Weight>
Weight BVTheorySolver<Weight>::refine_lbound_check(int bvID, Weight bound, Var ignore_bit){
#ifndef NDEBUG
	//test all values of mbits, find the lowest one >= i
	static vec<Weight> vals;
	vals.clear();

    dbg_evaluate(bvID,bitvectors[bvID].size()-1,vals,0);
    Weight highest_bound = -1;
    for(Weight x:vals){
    	if (x<= bound && (highest_bound<0 || x>highest_bound)){
    		highest_bound=x;
    	}
    }
    return highest_bound;

#endif
    return 0;
}

	template<typename Weight>
	Weight BVTheorySolver<Weight>::refine_ubound(int bvID, Weight bound, Var ignore_bit){
#ifndef NDEBUG
		Weight expected = refine_ubound_check(bvID,bound,ignore_bit);
#endif
		//Weight under_old = under_approx[bvID];
		//Weight over_old = over_approx[bvID];
		vec<Lit> & bv = bitvectors[bvID];
		bool done = false;
		//under_causes[bvID].clear();
		//over_causes[bvID].clear();
		int last_set_x =-1;
		int j = bv.size()-1;


		Weight proposed_bound=0;
		for(int i = bv.size()-1; i>=0 && ! done;i--){

			Weight ibit = ((Weight )1)<<i;
			if ((value(bv[i])!=l_True || var(bv[i])==ignore_bit) && (bound & (ibit))){
				bool found=false;
				while(j>=i){
					Weight jbit = ((Weight )1)<<j;
					if ((value(bv[j])==l_Undef || var(bv[j])==ignore_bit) && !(proposed_bound & jbit) ){
						//bv[j] must also not have been used yet as a flip value for this case (else we should try the (!(bound & (jbit))) case)...
						last_set_x=j;
					}else if  (!(bound & (jbit)) && ((value(bv[j])==l_True || var(bv[j])==ignore_bit || (proposed_bound & jbit)))){
						found=true;
						last_set_x=-1;
						//if j is > than i, then we are done.
						if (j>i){
							done=true;
							break;
						}
					}
					j--;
				}
				if (last_set_x  >-1){
					found=true;
					//#we were forced to set an xbit to 1 to match this ibit.
					Weight xbit = ((Weight )1)<<last_set_x;
					assert(!(proposed_bound & xbit));
					proposed_bound|=xbit;
					//mp[last_set_x]='s'
					if (last_set_x>i){
						//#mp is > i, so we are done
						done=true;
						break;
					}else{
						assert(last_set_x==i);
						j=bv.size()-1;//can we do better than this?
					}
				}

				if (! found){
					assert(expected==-1);
					return -1;
				}
			}
		}
		Weight refined_bound=lowest(bvID);
	    j =bv.size()-1;
		while (j>=0){
			Weight jbit = ((Weight )1)<<j;
			if (proposed_bound & jbit){
				assert(!(refined_bound & jbit));
				refined_bound |=jbit;
				if (!(bound & (jbit))){
					break;
				}

			}
			j--;
		}


	    if(refined_bound< bound){
	    	assert(expected==-1);
	        return -1;
	    }
	        //#return ubound(ibits,mbits2)
#ifndef NDEBUG
		if(refined_bound!=expected){
			assert(false);
			exit(5);
		}
#endif
	    return refined_bound;
	}
template<typename Weight>
Weight BVTheorySolver<Weight>::refine_lbound(int bvID, Weight obound, Var ignore_bit){
#ifndef NDEBUG
	Weight expected = refine_lbound_check(bvID,obound,ignore_bit);
#endif
	//Weight under_old = under_approx[bvID];
	//Weight over_old = over_approx[bvID];
	vec<Lit> & bv = bitvectors[bvID];
	bool done = false;
	//under_causes[bvID].clear();
	//over_causes[bvID].clear();
	int last_set_x =-1;
	int j = bv.size()-1;

	Weight bound = ~obound;

	Weight proposed_bound=0;
	for(int i = bv.size()-1; i>=0 && ! done;i--){

		Weight ibit = ((Weight )1)<<i;
		if ((value(bv[i])!=l_False || var(bv[i])==ignore_bit) && (bound & (ibit))){
			bool found=false;
			while(j>=i){
				Weight jbit = ((Weight )1)<<j;
				if ((value(bv[j])==l_Undef || var(bv[j])==ignore_bit) && !(proposed_bound & jbit) ){
					//bv[j] must also not have been used yet as a flip value for this case (else we should try the (!(bound & (jbit))) case)...
					last_set_x=j;
				}else if  (!(bound & (jbit)) && ((value(bv[j])==l_False || var(bv[j])==ignore_bit || (proposed_bound & jbit)))){
					found=true;
					last_set_x=-1;
					//if j is > than i, then we are done.
					if (j>i){
						done=true;
						break;
					}
				}
				j--;
			}
			if (last_set_x  >-1){
				found=true;
				//#we were forced to set an xbit to 1 to match this ibit.
				Weight xbit = ((Weight )1)<<last_set_x;
				assert(!(proposed_bound & xbit));
				proposed_bound|=xbit;
				//mp[last_set_x]='s'
				if (last_set_x>i){
					//#mp is > i, so we are done
					done=true;
					break;
				}else{
					assert(last_set_x==i);
					j=bv.size()-1;//can we do better than this?
				}
			}

			if (! found)
				return -1;
		}
	}
	Weight refined_bound= 0;//(bvID);
	for (int i = 0;i<bv.size();i++){
		if (value(bv[i])==l_False)
			refined_bound += ((Weight )1)<<i;
	}
	j =bv.size()-1;
	while (j>=0){
		Weight jbit = ((Weight )1)<<j;
		if (proposed_bound & jbit){
			assert(!(refined_bound & jbit));
			refined_bound |=jbit;
			if (!(bound & (jbit))){
				break;
			}

		}
		j--;
	}

	refined_bound = (1L<<bv.size()) + ~refined_bound;
	if(refined_bound> obound){
		assert(expected==-1);
		return -1;
	}

		//#return ubound(ibits,mbits2)

#ifndef NDEBUG
		if(refined_bound!=expected){
			assert(false);
			exit(5);
		}
#endif
	return refined_bound;
}
template<typename Weight>
using BitVector = typename BVTheorySolver<Weight>::BitVector;

}
;

#endif
