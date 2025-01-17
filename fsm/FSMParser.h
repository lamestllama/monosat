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

#ifndef FSM_PARSER_H_
#define FSM_PARSER_H_

#include <stdio.h>

#include "utils/ParseUtils.h"
#include "core/SolverTypes.h"
#include "fsm/FSMTheory.h"
#include <algorithm>
#include "core/Config.h"

#include "core/Dimacs.h"

#include <set>
#include <string>
#include <sstream>
namespace Monosat {


//=================================================================================================
// GRAPH Parser:
template<class B, class Solver>
class FSMParser: public Parser<B, Solver> {
	vec<int> fsmIDs;

	vec<int> inAlphabets;
	vec<int> outAlphabets;
	struct Transition{
		int fsm;
		int from;
		int to;
		int input;
		int output;
		Var edgeVar;
	};
	vec<bool> hasEpsilonTransitions;
	vec<vec<Transition> > transitions;
	vec<bool> created_strings;
	vec<vec<int>> strings;
	vec<int> stringLabels;
	struct Accepts{
		int fsm;
		int from;
		int to;
		int strID;
		Var reachVar;

	};
	vec<vec<Accepts> > accepts;

	struct ComposeAccepts{
		int fsmID1;
		int fsmID2;
		int from1;
		int from2;
		int to1;
		int to2;
		int strID;
		Var reachVar;

	};
	vec<ComposeAccepts>  compose_accepts;
	struct Generates{
		int fsm;
		int from;
		int strID;
		Var reachVar;
	};

	vec<vec<Generates> > generates;

	struct Transduces{
			int fsm;
			int from;
			int to;
			int strID;
			int strID2;
			Var reachVar;
		};

	vec<vec<Transduces> > transduces;


	vec<Lit> lits;
	int count = 0;

	void readFSM(B& in,  Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		int fsmID = parseInt(in);  //id of the fsm
		int n_labels = parseInt(in);
		bool hasEpsilon= parseInt(in)>0;
		if (fsmID < 0 ) {
			printf("PARSE ERROR! FSM id must be >=0, was %d\n", fsmID), exit(1);
		}
		if (n_labels<0){
			printf("PARSE ERROR! Number of transition labels must be >=0, was %d\n", n_labels), exit(1);
		}

		fsmIDs.growTo(fsmID + 1,-1);
		if(fsmIDs[fsmID]>=0){
			printf("PARSE ERROR! FSM id %d declared twice!\n", fsmID), exit(1);
		}
		//fsms[fsmID]= new FSMTheorySolver(&S);
		fsmIDs[fsmID]=fsmID;
		//S.addTheory(fsms[fsmID]);
		transitions.growTo(fsmID+1);
		accepts.growTo(fsmID+1);
		generates.growTo(fsmID+1);
		transduces.growTo(fsmID+1);
		hasEpsilonTransitions.growTo(fsmID+1);
		inAlphabets.growTo(fsmID+1,0);
		outAlphabets.growTo(fsmID+1,0);
	}
	
	void readString(B& in, Solver & S){
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		int strID = parseInt(in);
		strings.growTo(strID+1);
		created_strings.growTo(strID+1);
		stringLabels.growTo(strID+1);
		if(strID<0 || created_strings[strID]){
			printf("PARSE ERROR! Bad string id %d\n", strID), exit(1);
		}

		created_strings[strID]=true;

		//allow zero-length strings
		if (isEof(in) || *in == '\n')
			return;
		while(int i = parseInt(in)){
			if (i<=0){
				printf("PARSE ERROR! FSM strings must contain only positive (non-zero) integers, found %d\n", i), exit(1);
			}
			strings[strID].push(i);
			stringLabels[strID]= std::max(stringLabels[strID],i+1);
			skipWhitespace(in);
			if (isEof(in) || *in == '\n')
				break;

		}

	}

	void readTransition(B& in, Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}
		
		++in;
		
		int fsmID = parseInt(in);
		int from = parseInt(in);
		int to = parseInt(in);
		int input = parseInt(in);
		int output = parseInt(in);
		int edgeVar = parseInt(in) - 1;
		
		if (fsmID < 0 || fsmID >= fsmIDs.size()) {
			printf("PARSE ERROR! Undeclared fsm identifier %d for edge %d\n", fsmID, edgeVar), exit(1);
		}
		if (input<0){
			printf("PARSE ERROR! Transition inputs  must be >=0, was %d\n", input), exit(1);
		}
		if (output<0){
				printf("PARSE ERROR! Transition outputs  must be >=0, was %d\n", output), exit(1);
			}
		if (edgeVar < 0) {
			printf("PARSE ERROR! Edge variables must be >=0, was %d\n", edgeVar), exit(1);
		}

		if (input==0){
			hasEpsilonTransitions[fsmID]=true;
		}

		while (edgeVar >= S.nVars())
			S.newVar();
		
		inAlphabets[fsmID]=std::max(inAlphabets[fsmID],input);
		outAlphabets[fsmID]=std::max(outAlphabets[fsmID],output);
		transitions[fsmID].push({fsmID,from,to,input,output,edgeVar});
	}

	void readAccepts(B& in, Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		++in;
		
		int fsmID = parseInt(in);
		int from = parseInt(in);
		int to = parseInt(in);
		int strID = parseInt(in);
		int reachVar = parseInt(in) - 1;

		//now read in the string
		accepts[fsmID].push();

		accepts[fsmID].last().fsm=fsmID;
		accepts[fsmID].last().from=from;
		accepts[fsmID].last().to=to;
		accepts[fsmID].last().strID = strID;
		accepts[fsmID].last().reachVar = reachVar;

		if (fsmID < 0 || fsmID >= fsmIDs.size()) {
			printf("PARSE ERROR! Undeclared fsm identifier %d for edge %d\n", fsmID, reachVar), exit(1);
		}

		if (from<0){
				printf("PARSE ERROR! Source state must be a node id (a non-negative integer), was %d\n", from), exit(1);
			}
		if (to<0){
			printf("PARSE ERROR! Accepting state must be a node id (a non-negative integer), was %d\n", to), exit(1);
		}
		if (reachVar < 0) {
			printf("PARSE ERROR! Edge variables must be >=0, was %d\n", reachVar), exit(1);
		}
		
		while (reachVar >= S.nVars())
			S.newVar();


	}
	
	void readCompositionAccepts(B& in, Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		++in;

		int fsmID1 = parseInt(in);
		int fsmID2 = parseInt(in);
		int from1 = parseInt(in);
		int to1 = parseInt(in);
		int from2 = parseInt(in);
		int to2 = parseInt(in);
		int strID = parseInt(in);
		int reachVar = parseInt(in) - 1;

		//now read in the string
		compose_accepts.push();

		compose_accepts.last().fsmID1=fsmID1;
		compose_accepts.last().fsmID2=fsmID2;
		compose_accepts.last().from1=from1;
		compose_accepts.last().from2=from2;
		compose_accepts.last().to1=to1;
		compose_accepts.last().to2=to2;
		compose_accepts.last().strID = strID;
		compose_accepts.last().reachVar = reachVar;


		if (from1<0){
				printf("PARSE ERROR! Source state must be a node id (a non-negative integer), was %d\n", from1), exit(1);
			}
		if (to1<0){
			printf("PARSE ERROR! Accepting state must be a node id (a non-negative integer), was %d\n", to1), exit(1);
		}

		if (fsmID2 < 0 || fsmID2 >= fsmIDs.size()) {
			printf("PARSE ERROR! Undeclared fsm identifier %d for edge %d\n", fsmID2, reachVar), exit(1);
		}

		if (from2<0){
				printf("PARSE ERROR! Source state must be a node id (a non-negative integer), was %d\n", from2), exit(1);
			}
		if (to2<0){
			printf("PARSE ERROR! Accepting state must be a node id (a non-negative integer), was %d\n", to2), exit(1);
		}

		if (reachVar < 0) {
			printf("PARSE ERROR! Edge variables must be >=0, was %d\n", reachVar), exit(1);
		}

		while (reachVar >= S.nVars())
			S.newVar();

	}

	void readGenerates(B& in, Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		++in;

		int fsmID = parseInt(in);
		int from = parseInt(in);

		int strID = parseInt(in);
		int reachVar = parseInt(in) - 1;

		//now read in the string
		generates[fsmID].push();

		generates[fsmID].last().fsm=fsmID;
		generates[fsmID].last().from=from;

		generates[fsmID].last().strID = strID;
		generates[fsmID].last().reachVar = reachVar;

		if (fsmID < 0 || fsmID >= fsmIDs.size()) {
			printf("PARSE ERROR! Undeclared fsm identifier %d for edge %d\n", fsmID, reachVar), exit(1);
		}

		if (from<0){
				printf("PARSE ERROR! Source state must be a node id (a non-negative integer), was %d\n", from), exit(1);
			}

		if (reachVar < 0) {
			printf("PARSE ERROR! Edge variables must be >=0, was %d\n", reachVar), exit(1);
		}

		while (reachVar >= S.nVars())
			S.newVar();


	}

	void readTransduces(B& in, Solver& S) {
		if (opt_ignore_theories) {
			skipLine(in);
			return;
		}

		++in;

		int fsmID = parseInt(in);
		int from = parseInt(in);
		int to = parseInt(in);
		int strID = parseInt(in);
		int strID2 = parseInt(in);
		int reachVar = parseInt(in) - 1;

		//now read in the string
		transduces[fsmID].push();

		transduces[fsmID].last().fsm=fsmID;
		transduces[fsmID].last().from=from;
		transduces[fsmID].last().to=to;

		transduces[fsmID].last().strID = strID;
		transduces[fsmID].last().strID2 = strID2;
		transduces[fsmID].last().reachVar = reachVar;

		if (fsmID < 0 || fsmID >= fsmIDs.size()) {
			printf("PARSE ERROR! Undeclared fsm identifier %d for edge %d\n", fsmID, reachVar), exit(1);
		}

		if (from<0){
				printf("PARSE ERROR! Source state must be a node id (a non-negative integer), was %d\n", from), exit(1);
			}
		if (to<0){
				printf("PARSE ERROR! Accepting state must be a node id (a non-negative integer), was %d\n", to), exit(1);
			}
		if (reachVar < 0) {
			printf("PARSE ERROR! Edge variables must be >=0, was %d\n", reachVar), exit(1);
		}

		while (reachVar >= S.nVars())
			S.newVar();


	}

public:
	FSMParser(){
		
	}

	bool parseLine(B& in, Solver& S) {

		skipWhitespace(in);
		if (*in == EOF)
			return false;

		if (match(in, "fsm")) {
			skipWhitespace(in);
			readFSM(in,S);
			skipWhitespace(in);

			return true;
		} else if (match(in, "transition")) {
			count++;
			readTransition(in, S);
			return true;
		}else if (match(in,"str")){
			readString(in,S);
			return true;
		}else if (match(in, "accepts")) {
			readAccepts(in, S);
			return true;
		}else if (match(in,"generates")){
			readGenerates(in,S);
			return true;
		}else if (match(in,"transduces")){
			readTransduces(in,S);
			return true;
		}else if (match(in,"composition_accepts")){
			readCompositionAccepts(in,S);
			return true;
		}
		return false;
	}
	

	void implementConstraints(Solver & S) {
		FSMTheorySolver * theory=nullptr;

		for(int i = 0;i<fsmIDs.size();i++){
			int fsmID = fsmIDs[i];
			if(fsmID<0)
				continue;

			if(!theory){
				theory = new FSMTheorySolver(&S);
				S.addTheory(theory);
				theory->setStrings(&strings);
			}
				theory->newFSM(fsmID);

				theory->setAlphabets(fsmID,inAlphabets[i],outAlphabets[i]);

				for (auto &t:transitions[i]){
					theory->newTransition(fsmID,t.from,t.to,t.input,t.output,t.edgeVar);
				}

				for(auto & a: accepts[i]){

					if (a.strID<0 || !created_strings[a.strID]){
						printf("PARSE ERROR! String ID must be a non-negative integer, was %d\n", a.strID), exit(1);
					}
					if(a.from<0 || a.from>=theory->nNodes(fsmID)){
							printf("PARSE ERROR! %d is not a valid state\n", a.from), exit(1);
						}
					if(a.to<0 || a.to>=theory->nNodes(fsmID)){
						printf("PARSE ERROR! %d is not a valid state\n", a.to), exit(1);
					}

					theory->addAcceptLit(fsmID,a.from, a.to,a.strID,a.reachVar);
				}

				for(auto & a: generates[i]){

					if (a.strID<0 || !created_strings[a.strID]){
						printf("PARSE ERROR! String ID must be a non-negative integer, was %d\n", a.strID), exit(1);
					}
					if(a.from<0 || a.from>=theory->nNodes(fsmID)){
							printf("PARSE ERROR! %d is not a valid state\n", a.from), exit(1);
						}

					theory->addGenerateLit(fsmID,a.from, a.strID,a.reachVar);
				}

				for(auto & a: transduces[i]){

					if (a.strID<0 || !created_strings[a.strID]){
						printf("PARSE ERROR! String ID must be a non-negative integer, was %d\n", a.strID), exit(1);
					}
					if(a.from<0 || a.from>=theory->nNodes(fsmID)){
							printf("PARSE ERROR! %d is not a valid state\n", a.from), exit(1);
						}
					if(a.to<0 || a.to>=theory->nNodes(fsmID)){
									printf("PARSE ERROR! %d is not a valid state\n", a.from), exit(1);
								}
					theory->addTransduceLit(fsmID,a.from,a.to, a.strID,a.strID2,a.reachVar);
				}



		}
		for(auto & c: compose_accepts){
			if(!theory){
				printf("PARSE ERROR! No fsms declared!\n"), exit(1);
				exit(1);
			}

			theory->addComposeAcceptLit(c.fsmID1,c.fsmID2,c.from1,c.to1,c.from2,c.to2, c.strID,c.reachVar);
		}


	}

	
};

//=================================================================================================
}
;

#endif /* GRAPH_PARSER_H_ */
