/*
 * Dijkstra.h
 *
 *  Created on: 2013-05-28
 *      Author: sam
 */

#ifndef RAMAL_REPS_H_
#define RAMAL_REPS_H_

#include <vector>
#include "alg/Heap.h"
#include "graph/DynamicGraph.h"
#include "Reach.h"
#include "Distance.h"
#include "Dijkstra.h"
#include "core/Config.h"
#include <algorithm>

namespace dgl{
template<typename Weight = int,class Status=typename Distance<Weight>::NullStatus>
class RamalReps:public Distance<Weight>{
public:
	DynamicGraph & g;
	std::vector<Weight> & weights;
	Status &  status;
	int reportPolarity;
	bool reportDistance;
	int last_modification;
	int last_addition;
	int last_deletion;
	int history_qhead;

	int last_history_clear;

	int source;
	Weight INF;





	std::vector<Weight> old_dist;
	std::vector<int> changed;
	std::vector<bool> node_changed;
	std::vector<Weight> dist;
	std::vector<int> prev;
	struct DistCmp{
		std::vector<Weight> & _dist;
		 bool operator()(int a, int b)const{
			return _dist[a]<_dist[b];
		}
		 DistCmp(std::vector<Weight> & d):_dist(d){};
	};
	Heap<DistCmp> q;

	std::vector<int> edgeInShortestPathGraph;
	std::vector<int> delta;
	std::vector<int> changeset;
public:

	int stats_full_updates;
	int stats_fast_updates;
	int stats_fast_failed_updates;
	int stats_skip_deletes;
	int stats_skipped_updates;
	int stats_num_skipable_deletions;
	double mod_percentage;

	double stats_full_update_time;
	double stats_fast_update_time;
	RamalReps(int s,DynamicGraph & graph,std::vector<Weight> & weights,	Status &  status, int reportPolarity=0,bool reportDistance=false):g(graph),weights(weights),status(status),reportPolarity(reportPolarity),reportDistance(reportDistance), last_modification(-1),last_addition(-1),last_deletion(-1),history_qhead(0),last_history_clear(0),source(s),INF(0),q(DistCmp(dist)){

		mod_percentage=0.2;
		stats_full_updates=0;
		stats_fast_updates=0;
		stats_skip_deletes=0;
		stats_skipped_updates=0;
		stats_full_update_time=0;
		stats_fast_update_time=0;
	}
	//Dijkstra(const Dijkstra& d):g(d.g), last_modification(-1),last_addition(-1),last_deletion(-1),history_qhead(0),last_history_clear(0),source(d.source),INF(0),q(DistCmp(dist)),stats_full_updates(0),stats_fast_updates(0),stats_skip_deletes(0),stats_skipped_updates(0),stats_full_update_time(0),stats_fast_update_time(0){marked=false;};


	void setSource(int s){
		source = s;
		last_modification=-1;
		last_addition=-1;
		last_deletion=-1;
	}
	int getSource(){
		return source;
	}

	std::vector<int> & getChanged(){
		return changed;
	}
	void clearChanged(){
		changed.clear();
	}

	void drawFull(){

	}

	void dbg_delta(){
#ifndef NDEBUG
		dbg_delta_lite();
		assert(delta.size()==g.nodes());

		for(int i = 0;i<g.nEdgeIDs();i++){
			if(!g.edgeEnabled(i)){
				assert(!edgeInShortestPathGraph[i]);
			}
		}

		std::vector<int> dbg_delta;
		std::vector<Weight> dbg_dist;
		dbg_dist.resize(g.nodes(),INF);
		dbg_delta.resize(g.nodes());
		dbg_dist[getSource()]=0;
		struct DistCmp{
			std::vector<Weight> & _dist;
			 bool operator()(int a, int b)const{
				return _dist[a]<_dist[b];
			}
			 DistCmp(std::vector<Weight> & d):_dist(d){};
		};
		Heap<DistCmp> q(dbg_dist);

		q.insert(getSource());

		while(q.size()){
			int u = q.removeMin();
			if(dbg_dist[u]==INF)
				break;
			dbg_delta[u]=0;

			for(int i = 0;i<g.nIncoming(u);i++){
					if(!g.edgeEnabled( g.incoming(u,i).id))
						continue;

					int edgeID = g.incoming(u,i).id;
					int v = g.all_edges[edgeID].from;
					Weight alt = dbg_dist[v]+ weights[edgeID];
					assert(alt>=dbg_dist[u]);
				/*	if (alt==dbg_dist[u]){
						dbg_delta[u]++;
					}*/
				}

			for(int i = 0;i<g.nIncident(u);i++){
				if(!g.edgeEnabled( g.incident(u,i).id))
					continue;

				int edgeID = g.incident(u,i).id;
				int v = g.all_edges[edgeID].to;
				Weight alt = dbg_dist[u]+ weights[edgeID];
				if(alt<dbg_dist[v]){

					dbg_dist[v]=alt;

					if(!q.inHeap(v))
						q.insert(v);
					else
						q.decrease(v);
				}/*else if (alt==dbg_dist[v]){
					dbg_delta[v]++;
				}*/
			}
		}

		for(int u = 0;u<g.nodes();u++){
			Weight & d = dist[u];

			assert(dbg_dist[u]==dist[u]);
			for(int i = 0;i<g.nIncoming(u);i++){
				if(!g.edgeEnabled( g.incoming(u,i).id))
					continue;

				int edgeID = g.incoming(u,i).id;
				int v = g.all_edges[edgeID].from;

				Weight alt = dbg_dist[v]+ weights[edgeID];
				assert(alt>=dbg_dist[u]);
				if(alt==dbg_dist[u]){
					dbg_delta[u]++;//is this right?
					assert(edgeInShortestPathGraph[edgeID]);
				}else{
					assert(!edgeInShortestPathGraph[edgeID]);
				}

			}

		}
		for(int u = 0;u<g.nodes();u++){
			Weight& d = dist[u];
			Weight& d_expect = dbg_dist[u];
			assert(d==dbg_dist[u]);

		}
		for(int u = 0;u<g.nodes();u++){
			int d = delta[u];
			int d_expect = dbg_delta[u];
			assert(d==dbg_delta[u]);

		}
		dbg_delta_lite();
#endif
	}

	void GRRInc(int edgeID){
		static int iter = 0;
		++iter;
		dbg_delta_lite();
		assert(g.edgeEnabled(edgeID));
		if(edgeInShortestPathGraph[edgeID])
			return;
		int ru =  g.all_edges[edgeID].from;
		int rv =  g.all_edges[edgeID].to;

		Weight & rdv = dist[rv];
		Weight & rdu = dist[ru];

		Weight& weight = weights[edgeID];
		if(dist[rv]<dist[ru]+weight)
			return;
		else if (dist[rv]==dist[ru]+weight ){
			assert(!edgeInShortestPathGraph[edgeID]);
			edgeInShortestPathGraph[edgeID]=true;
			delta[rv]++;//we have found an alternative shortest path to v
			return;
		}
		edgeInShortestPathGraph[edgeID]=true;
		delta[rv]++;
		dist[rv]=dist[ru]+weight;
		q.clear();
		q.insert(rv);

		while(q.size()){

			int u = q.removeMin();

			if(!node_changed[u]){
				node_changed[u]=true;
				changed.push_back(u);
			}
			delta[u]=0;
			//for(auto & e:g.inverted_adjacency[u]){
			for(int i = 0;i<g.nIncoming(u);i++){
				auto & e =g.incoming(u,i);
				int adjID = e.id;
				if (g.edgeEnabled(adjID)){

						assert(g.all_edges[adjID].to==u);
						int v =g.all_edges[adjID].from;
						Weight & w = weights[adjID];//assume a weight of one for now
						Weight & du = dist[u];
						Weight & dv = dist[v];
						if(dist[u]==dist[v]+w ){
							edgeInShortestPathGraph[adjID]=true;
							delta[u]++;
						}else if (dist[u]<(dist[v]+w)){
							//This doesn't hold for us, because we are allowing multiple edges to be added at once.
							//assert(dist[u]<(dist[v]+w));

							edgeInShortestPathGraph[adjID]=false;
						}else{
							//don't do anything. This will get corrected in a future call to GRRInc.
							//assert(false);

						}
				}else{
					edgeInShortestPathGraph[adjID]=false;//need to add this, because we may have disabled multiple edges at once.
				}
			}

			for(int i = 0;i<g.nIncident(u);i++){
				auto & e =g.incident(u,i);
				int adjID = e.id;
				if (g.edgeEnabled(adjID)){
						assert(g.all_edges[adjID].from==u);
						int s =g.all_edges[adjID].to;
						Weight & w = weights[adjID];//assume a weight of one for now
						Weight & du = dist[u];
						Weight & ds = dist[s];
						if(dist[s]>dist[u]+w){
							dist[s]=dist[u]+w;
							q.update(s);
						}else if (dist[s]==dist[u]+w && !edgeInShortestPathGraph[adjID]){
							edgeInShortestPathGraph[adjID]=true;
							delta[s]++;
						}
				}
			}
		}
		dbg_delta_lite();
	}
	void dbg_delta_lite(){
#ifndef NDEBUG
		for(int u = 0;u<g.nodes();u++){
			int del = delta[u];
			Weight d = dist[u];
			int num_in = 0;
			for(int i = 0;i<g.nIncoming(u);i++){
				auto & e =g.incoming(u,i);
				int adjID = e.id;
				int from = g.all_edges[adjID].from;

				Weight dfrom = dist[from];
				if(edgeInShortestPathGraph[adjID])
					num_in++;
			}
			assert(del==num_in);
		}
#endif

	}

	void GRRDec(int edgeID){
		dbg_delta_lite();
		assert(!g.edgeEnabled(edgeID));
		//First, check if this edge is actually in the shortest path graph
		if(!edgeInShortestPathGraph[edgeID])
			return;
		edgeInShortestPathGraph[edgeID]=false;//remove this edge from the shortest path graph

		int ru =  g.all_edges[edgeID].from;
		int rv =  g.all_edges[edgeID].to;
		assert(delta[rv]>0);
		delta[rv]--;
		if(delta[rv]>0)
			return; //the shortest path hasn't changed in length, because there was an alternate route of the same length to this node.

		q.clear();
		changeset.clear();
		changeset.push_back(rv);

		//find all effected nodes whose shortest path lengths may now be increased (or that may have become unreachable)
		for(int i = 0;i<changeset.size();i++){
			int u = changeset[i];
			dist[u] = INF;
			for(int i = 0;i<g.nIncident(u);i++){
				auto & e =g.incident(u,i);
				int adjID = e.id;
				if (g.edgeEnabled(adjID)){
					if(edgeInShortestPathGraph[adjID]){
						edgeInShortestPathGraph[adjID]=false;
						assert(g.all_edges[adjID].from==u);
						int s =g.all_edges[adjID].to;
						assert(delta[s]>0);
						delta[s]--;
						if(delta[s]==0){
							changeset.push_back(s);
						}
					}
				}
			}
		}

		for (int i = 0;i<changeset.size();i++){
			int u = changeset[i];
			assert(dist[u]==INF);
			for(int i = 0;i<g.nIncoming(u);i++){
				auto & e =g.incoming(u,i);
				int adjID = e.id;

				if (g.edgeEnabled(adjID)){
					assert(g.all_edges[adjID].to==u);
					int v =g.all_edges[adjID].from;
					Weight & w = weights[adjID];//assume a weight of one for now
					Weight alt = dist[v]+w;
					assert(!edgeInShortestPathGraph[adjID]);
					if(dist[u]>alt){
						dist[u]=alt;
					}
				}

			}
			if(dist[u]!=INF){
				//q.insert(u);
				//dbg_Q_add(q,u);
				q.insert(u);

				if(!reportDistance && reportPolarity>=0){
					if(!node_changed[u]){
						node_changed[u]=true;
						changed.push_back(u);
					}
				}
			}else if ( reportPolarity<=0){
				//have to mark this change even if we are reporting distanec, as u has not been added to the queue.
				if(!node_changed[u]){
					node_changed[u]=true;
					changed.push_back(u);
				}
			}
		}

		while(q.size()>0){
			int u = q.removeMin();
			if(reportDistance){
				if(dist[u]!=INF){
					if(reportPolarity>=0){
						if(!node_changed[u]){
							node_changed[u]=true;
							changed.push_back(u);
						}
					}
				}else if (reportPolarity<=0){
					if(!node_changed[u]){
						node_changed[u]=true;
						changed.push_back(u);
					}
				}
			}
			for(int i = 0;i<g.nIncident(u);i++){
				auto & e =g.incident(u,i);
				int adjID = e.id;
				if (g.edgeEnabled(adjID)){
					assert(g.all_edges[adjID].from==u);
					int s =g.all_edges[adjID].to;
					Weight w = weights[adjID];//assume a weight of one for now
					Weight alt = dist[u]+w;
					if(dist[s]>alt){
						if(reportPolarity>=0 && dist[s]>=0){
							//This check is needed (in addition to the above), because even if we are NOT reporting distances, it is possible for a node that was previously not reachable
							//to become reachable here. This is ONLY possible because we are batching multiple edge incs/decs at once (otherwise it would be impossible for removing an edge to decrease the distance to a node).
							if(!node_changed[s]){
								node_changed[s]=true;
								changed.push_back(s);
							}
						}

						dist[s]=alt;
						q.update(s);
					}else if (dist[s]==alt && !edgeInShortestPathGraph[adjID] ){
						edgeInShortestPathGraph[adjID]=true;
						delta[s]++;//added by sam... not sure if this is correct or not.
					}
				}
			}

			for(int i = 0;i<g.nIncoming(u);i++){
				auto & e =g.incoming(u,i);
				int adjID = e.id;
				if (g.edgeEnabled(adjID)){

						assert(g.all_edges[adjID].to==u);
						int v =g.all_edges[adjID].from;
						Weight & dv = dist[v];
						Weight & du = dist[u];
						bool edgeIn = edgeInShortestPathGraph[adjID];
						Weight & w = weights[adjID];//assume a weight of one for now
						if(dist[u]==dist[v]+w && ! edgeInShortestPathGraph[adjID]){
							assert(!edgeInShortestPathGraph[adjID]);
							edgeInShortestPathGraph[adjID]=true;
							delta[u]++;
						}else if (dist[u]<dist[v]+w &&  edgeInShortestPathGraph[adjID]){
							 edgeInShortestPathGraph[adjID]=false;
							 delta[u]--;
							assert(!edgeInShortestPathGraph[adjID]);
						}else if(dist[u]>dist[v]+w) {
							//assert(false);
						}
				}
			}
		}
		dbg_delta_lite();
	}




	void update( ){
#ifdef RECORD
		if(g.outfile){
			fprintf(g.outfile,"r %d\n", getSource());
		}
#endif
		static int iteration = 0;
		int local_it = ++iteration ;
		if(local_it==7668){
			int a =1;
		}
		if(last_modification>0 && g.modifications==last_modification)
				return;
		if(last_modification<=0 || g.changed()){
			INF=1;//g.nodes()+1;
			for(Weight & w:weights)
				INF+=w;
			dist.resize(g.nodes(),INF);
			dist[getSource()]=0;
			delta.resize(g.nodes());
			node_changed.resize(g.nodes(),true);

			for(int i = 0;i<g.nodes();i++){
				if((dist[i]>=INF && reportPolarity<=0) || (dist[i]<INF && reportPolarity>=0)){
				node_changed[i]=true;
				changed.push_back(i);//On the first round, report status of all nodes.
				}
			}
		}
		edgeInShortestPathGraph.resize(g.nEdgeIDs());
		if(last_history_clear!=g.historyclears){
			history_qhead=0;
			last_history_clear=g.historyclears;

		}
		
		for (int i = history_qhead;i<g.history.size();i++){
			int edgeid = g.history[i].id;
			if(g.history[i].addition && g.edgeEnabled(edgeid)){
				GRRInc(edgeid);
			}else if (!g.history[i].addition &&  !g.edgeEnabled(edgeid)){
				GRRDec(edgeid);
			}
		}

		//for(int i = 0;i<g.nodes();i++){
		//	int u=i;
		for(int u:changed){
			//int u = changed[i];
			node_changed[u]=false;
			//CANNOT clear the change flag here, because if we backtrack and then immediately re-propagate an edge before calling theoryPropagate, the change to this node may be missed.

			if(reportPolarity<=0 && dist[u]>=INF){
				status.setReachable(u,false);
				status.setMininumDistance(u,dist[u]<INF,dist[u]);
			}else if (reportPolarity>=0 && dist[u]<INF){
				status.setReachable(u,true);
				status.setMininumDistance(u,dist[u]<INF,dist[u]);
			}
		}
		changed.clear();
		//}
		assert(dbg_uptodate());

		last_modification=g.modifications;
		last_deletion = g.deletions;
		last_addition=g.additions;

		history_qhead=g.history.size();
		last_history_clear=g.historyclears;

		;
	}

	bool dbg_path(int to){
#ifdef DEBUG_DIJKSTRA
		assert(connected(to));
		if(to == source){
			return true;
		}
		int p = previous(to);

		if(p<0){
			return false;
		}
		if(p==to){
			return false;
		}

		return dbg_path(p);


#endif
		return true;
	}
	bool dbg_uptodate(){
#ifdef DEBUG_DIJKSTRA
		if(last_modification<0)
			return true;
		dbg_delta();
		Dijkstra<Weight> d(source,g,weights);

		for(int i = 0;i<g.nodes();i++){
			Weight dis = dist[i];
			bool c = i<dist.size() && dist[i]<INF;
			if(!c)
				dis = this->unreachable();

			Weight dbgdist = d.distance(i);

			if(dis!=dbgdist){
				assert(false);
				exit(4);
			}
		}
//#endif
#endif
		return true;
	}

	bool connected_unsafe(int t){
		dbg_uptodate();
		return t<dist.size() && dist[t]<INF;
	}
	bool connected_unchecked(int t){
		assert(last_modification==g.modifications);
		return connected_unsafe(t);
	}
	bool connected(int t){
		if(last_modification!=g.modifications)
			update();

		assert(dbg_uptodate());

		return dist[t]<INF;
	}
	Weight & distance(int t){
		if(last_modification!=g.modifications)
					update();
		if(connected_unsafe(t))
			return dist[t];
		else
			return this->unreachable();
	}
	Weight &distance_unsafe(int t){
		if(connected_unsafe(t))
			return dist[t];
		else
			return this->unreachable();;
	}
	int incomingEdge(int t){
/*
		assert(false);//not yet implemented...
		assert(t>=0 && t<prev.size());
		assert(prev[t]>=-1 );
		return prev[t];*/
		//not supported
		assert(false);
		exit(1);
	}
	int previous(int t){
		/*if(prev[t]<0)
			return -1;

		assert(g.all_edges[incomingEdge(t)].to==t);
		return g.all_edges[incomingEdge(t)].from;*/
		assert(false);
		exit(1);
	}
};
};
#endif
