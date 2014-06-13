
#ifndef SPIRA_PAN_H_
#define SPIRA_PAN_H_

#include "mtl/Vec.h"
#include "mtl/Heap.h"
#include "mtl/Sort.h"
#include "DynamicGraph.h"
#include "core/Config.h"
#include "MinimumSpanningTree.h"
#include "Kruskal.h"
#include <algorithm>
#include <limits>
using namespace Minisat;

/**
 * This is an implementation of Spira and Pan's (1975) dynamic minimum spanning tree algorithm.
 * We initialize the MST in new graphs using Prim's, and also use Prim's to connect separated components back
 * together after a string of edge deletions (the original paper only considers a single edge deletion at a time, which
 * is very ineficient if multiple edges are deleted at once).
 */
template<class Status,class EdgeStatus=DefaultEdgeStatus>
class SpiraPan:public MinimumSpanningTree{
public:

	DynamicGraph<EdgeStatus> & g;
	Status &  status;
	int last_modification;
	int min_weight;
	int last_addition;
	int last_deletion;
	int history_qhead;

	int last_history_clear;

	int INF;

	vec<int> mst;
	vec<int> q;
	vec<int> check;
	const int reportPolarity;

	//vec<char> old_seen;
	vec<bool> in_tree;
	vec<bool> keep_in_tree;
	vec<int> parents;
	vec<int> parent_edges;
	vec<int> components_to_visit;
	vec<int> component_member;//pointer to one arbitrary member of each non-empty component
	vec<int> component_weight;
    struct VertLt {
        const vec<int>&  keys;

        bool operator () (int x, int y) const {
        	return keys[x]<keys[y];
        }
        VertLt(const vec<int>&  _key) : keys(_key) { }
    };

	Heap<VertLt> Q;

	vec<bool> seen;

	int num_sets=0;

	vec<int> edge_to_component;
	vec<int> empty_components;//list of component ids with no member nodes
	vec<int> components;

	struct DefaultReachStatus{
			vec<bool> stat;
				void setReachable(int u, bool reachable){
					stat.growTo(u+1);
					stat[u]=reachable;
				}
				bool isReachable(int u) const{
					return stat[u];
				}
				DefaultReachStatus(){}
			};
#ifndef NDEBUG
	Kruskal<MinimumSpanningTree::NullStatus,EdgeStatus> dbg;
#endif
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

	SpiraPan(DynamicGraph<EdgeStatus> & graph, Status & status, int reportPolarity=0 ):g(graph), status(status), last_modification(-1),last_addition(-1),last_deletion(-1),history_qhead(0),last_history_clear(0),INF(0),reportPolarity(reportPolarity),Q(VertLt(component_weight))
#ifndef NDEBUG
		,dbg(g,MinimumSpanningTree::nullStatus,0)
#endif
	{
		mod_percentage=0.2;
		stats_full_updates=0;
		stats_fast_updates=0;
		stats_skip_deletes=0;
		stats_skipped_updates=0;
		stats_full_update_time=0;
		stats_fast_update_time=0;
		stats_num_skipable_deletions=0;
		stats_fast_failed_updates=0;
		min_weight=-1;

	}

	void setNodes(int n){
		q.capacity(n);
		check.capacity(n);
		in_tree.growTo(g.nEdgeIDs());
		seen.growTo(n);
		INF=std::numeric_limits<int>::max();
		component_weight.growTo(g.nodes,INF);
		parents.growTo(n,-1);
		edge_to_component.growTo(n,-1);

		parent_edges.growTo(n,-1);
	}
/*
 * 	//chin houck insertion
	void insert(int newNode){
		if(g.adjacency_undirected[newNode].size()==0)
			return;
		marked.clear();
		marked.growTo(g.nodes);
		incident_edges.clear();
		incident_edges.growTo(g.nodes,-1);
		for(auto & edge:g.adjacency_undirected[newNode]){
			incident_edges[edge.node]=edge.id;
		}
		insert(g.adjacency_undirected[newNode][0]);
	}

	//Insert a node into an _existing_ minimum spanning tree
	void insert(int r, int z, int & t){
		int m = incident_edges[r];
		marked[r]=true;

		for(auto edge:g.adjacency_undirected[r]){//IF this is a dense graph, then this loop is highly sub-optimal!
			//for each incident edge in the old MST:
			if(in_tree[edge.id] && !marked[ edge.node]){
				insert(edge.node,z,t);
				assert(dbg_is_largest_edge_on_path(m,r,z));
				assert(dbg_is_largest_edge_on_path(t,edge.node,z));
				int k = t;
				if(t<0 || g.weights[edge.id]>g.weights[t])
					k=edge.id;
				int h = t;
				if(t<0 || g.weights[edge.id]<g.weights[t])
					h=edge.id;
				if(h>=0){
					keep_in_tree[h]=true;
				}
				if(m<0 || (k>=0 && g.weights[k]<g.weights[m])){
					m=k;
				}
			}
		}
		t=m;
	}*/

	bool dbg_is_largest_edge_on_path(int edge, int from, int to){
#ifndef NDEBUG

#endif
		return true;
	}

	void dbg_parents(){
#ifndef NDEBUG
		//check that the parents don't cycle
		for(int i = 0;i<g.nodes;i++){
			int p = i;
			int num_parents= 0;
			while(p!=-1){
				num_parents++;
				if(parents[p]>-1){
					int e = parent_edges[p];
					int u = g.all_edges[e].from;
					int v = g.all_edges[e].to;
					assert(u==p || v==p);
					assert(u==parents[p]||v==parents[p]);
					assert(in_tree[e]);
					int compi = components[i];
					int compp = components[p];
					assert(components[p]==components[i]);
					int parent = parents[p];
					int pc = components[parents[p]];
					assert(components[parents[p]]==components[i]);
				}else{
					assert(parent_edges[p]==-1);
				}

				p = parents[p];
				assert(num_parents<=g.nodes);

			}

		}

		vec<int> used_components;
		for(int i = 0;i<g.nodes;i++){
			if(!used_components.contains(components[i])){
				used_components.push(components[i]);

			}
		}

		for(int c:used_components){
			assert(!empty_components.contains(c));
			assert(component_member[c]!=-1);
			int m = component_member[c];
			assert(components[m]== c);
		}

		for(int c :empty_components){
			assert(!used_components.contains(c));
			assert(component_member[c]==-1);
		}




#endif
	}



	void addEdgeToMST(int edgeid){
		int u = g.all_edges[edgeid].from;
		int v = g.all_edges[edgeid].to;
		int w = g.all_edges[edgeid].weight;
		dbg_parents();
		if(components[u] != components[v]){

			//If u,v are in separate components, then this edge must be in the mst (and we need to fix the component markings)
			in_tree[edgeid]=true;
			num_sets--;
			int higher_component = v;
			int lower_component = u;
			int new_c = components[u];
			int old_c = components[v];
			if(new_c>old_c){
				std::swap(higher_component,lower_component);
				std::swap(new_c,old_c);
			}
			//ok, now set every node in the higher component to be in the lower component with a simple dfs.
			//fix the parents at the same time.
			min_weight+=g.weights[edgeid];
			assert(components[lower_component]==new_c);
			assert(components[higher_component]==old_c);
			components[higher_component]=new_c;
			parents[higher_component]=lower_component;
			parent_edges[higher_component]=edgeid;
			q.clear();
			q.push(higher_component);
			while(q.size()){
				int n = q.last(); q.pop();
				for(auto & edge:g.adjacency_undirected[n]){
					if(in_tree[edge.id]){

						int t = edge.node;
						if(components[t]==old_c){
							components[t]=new_c;
							parents[t]=n;
							parent_edges[t]=edge.id;
							q.push(t);
						}

					}
				}
			}
			component_member[old_c]=-1;
			empty_components.push(old_c);
			dbg_parents();
		}else{
			dbg_parents();
			assert(components[u]==components[v]);
			if(parents[u]==v || parents[v]==u){
				//If there is already another edge (u,v) that is in the tree, then we at most need to swap that edge out for this one.
				//note that this can only be the case if u is the parent of v or vice versa
				int p_edge = parent_edges[u];
				if(parents[v]==u){
					p_edge = parent_edges[v];
				}
				if(g.getWeight(p_edge)> g.getWeight(edgeid)){
					//then swap these edges without changing anything else.
					in_tree[p_edge]=false;
					in_tree[edgeid]=true;
					int delta = g.getWeight(p_edge)- g.getWeight(edgeid);
					min_weight-=delta;
					if(parents[v]==u){
						assert(parent_edges[v]==p_edge);
						parent_edges[v] = edgeid;
					}else{
						assert(parent_edges[u]==p_edge);
						parent_edges[u] = edgeid;
					}
				}
				dbg_parents();
			}else{
				//otherwise, find the cycle induced by adding this edge into the MST (by walking up the tree to find the LCA - if we are doing many insertions, could we swap this out for tarjan's OLCA?).
				int p = u;

				while(p>-1){
					seen[p]=true;
					p=parents[p];
				}
				int max_edge_weight = -1;
				int max_edge = -1;
				bool edge_on_left=false;//records which branch of the tree rooted at p the edge we are replacing is
				p = v;
				while(true){
					assert(p>-1);//u and v must share a parent, because u and v are in the same connected component and we have already computed the mst.
					if(seen[p]){
						break;
					}else{
						if (parents[p]>-1 && g.getWeight( parent_edges[p]) > max_edge_weight){
							assert( parent_edges[p]>-1);
							max_edge_weight=g.weights[ parent_edges[p]];
							max_edge = parent_edges[p];
						}
						p = parents[p];
					}
				}
				assert(seen[p]);
				int lca = p;//this is the lowest common parent of u and v.
				 p = u;
				while(p!=lca){
					assert(seen[p]);
					seen[p]=false;
					if (parents[p]>-1 && g.getWeight( parent_edges[p]) > max_edge_weight){
						assert( parent_edges[p]>-1);
						max_edge_weight=g.weights[ parent_edges[p]];
						max_edge = parent_edges[p];
						edge_on_left=true;
					}
					p=parents[p];
				}

				//reset remaining 'seen' vars
				assert(p==lca);
				while (p>-1){
					assert(seen[p]);
					seen[p]=false;
					p = parents[p];
				}
				assert(max_edge>-1);
				if(max_edge_weight>g.getWeight(edgeid)){
					//then swap out that edge with the new edge.
					//this will also require us to repair parent edges in the cycle
					min_weight-= max_edge_weight;
					min_weight+= g.getWeight(edgeid);
					in_tree[edgeid]=true;
					in_tree[max_edge]=false;

					int last_p;
					if(edge_on_left){
						p = u;
						last_p = v;
					}else{
						p=v;
						last_p = u;
					}

					int last_edge = edgeid;
					while(parent_edges[p]!=max_edge){
						assert(p>-1);
						int next_p = parents[p];
						std::swap(parent_edges[p],last_edge);//re-orient the parents.
						parents[p]=last_p;
						last_p = p;
						p = next_p;
					}
					assert(parent_edges[p]==max_edge);
					std::swap(parent_edges[p],last_edge);//re-orient the parents.
					parents[p]=last_p;
				}
				dbg_parents();
			}

		}
		dbg_parents();
	}

	void removeEdgeFromMST(int edgeid){
		dbg_parents();
		if(!in_tree[edgeid]){

			//If an edge is disabled that was NOT in the MST, then no update is required.
		}else{
			//this is the 'tricky' case for dynamic mst.
			//following Spira & Pan, each removed edge splits the spanning tree into separate components that are MST's for those components.
			//after all edges that will be removed have been removed, we will run Prim's to stitch those components back together, if possible.

			int u = g.all_edges[edgeid].from;
			int v = g.all_edges[edgeid].to;

			in_tree[edgeid]=false;
			min_weight-=g.getWeight(edgeid);
			num_sets++;

			assert(components[u]==components[v]);
			assert(parents[u]==v || parents[v]==u);

			if(parents[u]==v){
				parents[u]=-1;
				parent_edges[u]=-1;
			}else{
				parents[v]=-1;
				parent_edges[v]=-1;
			}
			int start_node= v;
			//if we want to maintain the guarantee components are always assigned the lowest node number that they contain, we'd need to modify the code below a bit.
			assert(empty_components.size());
			int new_c = empty_components.last();
			empty_components.pop();
			int old_c = components[u];
			assert(component_member[new_c]==-1);
			component_member[new_c]=start_node;
			component_member[old_c]=u;
			components_to_visit.push(new_c);

			components[start_node]=new_c;
			assert(q.size()==0);
			//relabel the components of the tree that has been split off.
			q.clear();
			q.push(start_node);
			while(q.size()){
				int n = q.last(); q.pop();
				for(auto & edge:g.adjacency_undirected[n]){
					if(in_tree[edge.id] ){
						//assert(g.edgeEnabled(edge.id));
						int t = edge.node;
						if(components[t]==old_c){
							components[t]=new_c;
							q.push(t);
						}
					}
				}
			}

		}
		dbg_parents();
	}

	void prims(){
		dbg_parents();
		//component_weight.clear();

		for(int i = 0;i<components_to_visit.size();i++){
			int c = components_to_visit[i];
			int start_node = component_member[c];
			if(start_node==-1){
				//then this component has already been merged into another one, no need to visit it.
				continue;
			}
			assert(c>=0);
#ifndef NDEBUG
			for(int w:component_weight){
				assert(w==INF);
			}
#endif
			//ok, try to connect u's component to v
			//ideally, we'd use the smaller of these two components...
			int smallest_edge=-1;
			int smallest_weight = INF;
			Q.insert(c);

			//do a dfs to find all the edges leading out of this component.
			while(Q.size()){
				int cur_component = Q.removeMin();

				int last_p = -1;
				int last_edge = -1;

				if(cur_component!=c){
					//connect these two components together
					int edgeid = edge_to_component[cur_component];
					assert(g.getWeight(edgeid)==component_weight[cur_component]);
					int u = g.all_edges[edgeid].from;
					int v=  g.all_edges[edgeid].to;
					assert(components[u]==c||components[v]==c);
					assert(components[u]==cur_component||components[v]==cur_component);

					last_edge=edgeid;
					if(components[u]==cur_component){
						//attach these components together using this edge.
						//this will force us to re-root cur_component at u (this will happen during the dfs below)
						start_node= u;
						last_p = v;
					}else{
						start_node = v;
						last_p=u;
					}

					num_sets--;
					in_tree[edgeid]=true;
					min_weight+=g.getWeight(edgeid);
					parents[start_node]=last_p;
					parent_edges[start_node]=edgeid;
					assert(components[start_node]==cur_component);
					components[start_node]=c;
					component_member[cur_component]=-1;
					empty_components.push(cur_component);
				}
				component_weight[cur_component]=INF;
				q.clear();
				q.push(start_node);
				seen[start_node]=true;
				//do a bfs over the component, finding all edges that leave the component. fix the parent edges in the same pass, if needed.
				//could also do a dfs here - would it make a difference?
				for(int i = 0;i<q.size();i++){
					int n = q[i];
					assert(components[n]==c);
					for(auto & edge:g.adjacency_undirected[n]){
						if(g.edgeEnabled(edge.id)){
							int t = edge.node;
							if(!in_tree[edge.id]){
								//assert(g.edgeEnabled(edge.id));
								int ncomponent = components[t];
								if(ncomponent!= c && ncomponent != cur_component){
									int w = g.getWeight(edge.id);
									if(w<component_weight[ncomponent]){

										edge_to_component[ncomponent]=edge.id;
										component_weight[ncomponent]= w;

										Q.update(ncomponent);
									}
								}
							}else if (components[t]==cur_component){
								if(components[t]!=c){
									components[t]=c;
									parents[t]=n;
									parent_edges[t]=edge.id;
								}
								if(!seen[t]){//can we avoid this seen check?
									seen[t]=true;
									q.push(t);
								}
							}
						}
					}
				}
				for(int s:q){
					assert(seen[s]);
					seen[s]=false;
				}
				q.clear();
#ifndef NDEBUG
				for(bool b:q)
					assert(!b);
#endif

			}
		}
		components_to_visit.clear();
	}

	void update( ){
		static int iteration = 0;
		int local_it = ++iteration ;
#ifdef RECORD
		if(g.outfile && mstalg==MinSpanAlg::ALG_SPIRA_PAN){
			fprintf(g.outfile,"m\n");
			fflush(g.outfile);
		}
#endif
		if(last_modification>0 && g.modifications==last_modification)
					return;
		assert(components_to_visit.size()==0);
		if(last_modification<=0 || g.changed() || last_history_clear!=g.historyclears){
			INF=g.nodes+1;


			setNodes(g.nodes);
			seen.clear();
			seen.growTo(g.nodes);
			min_weight=0;
			num_sets = g.nodes;
			empty_components.clear();
			components.clear();
			for(int i = 0;i<g.nodes;i++){
				components.push(i);
				components_to_visit.push(i);
			}
			component_weight.clear();
			component_weight.growTo(g.nodes,INF);
			component_member.clear();
			for(int i = 0;i<g.nodes;i++)
				component_member.push(i);
			mst.clear();
			parents.clear();
			parents.growTo(g.nodes,-1);
			parent_edges.clear();
			parent_edges.growTo(g.edges,-1);
			for(int i = 0;i<in_tree.size();i++)
				in_tree[i]=false;
			last_history_clear=g.historyclears;
			history_qhead=g.history.size();//have to skip any additions or deletions that are left here, as otherwise the tree wont be an MST at the beginning of the addEdgeToMST method, which is an error.

		}


		double startdupdatetime = rtime(2);

		for (int i = history_qhead;i<g.history.size();i++){

			int edgeid = g.history[i].id;
			if(g.history[i].addition && g.edgeEnabled(edgeid) ){
				addEdgeToMST(edgeid);
			}else if (!g.history[i].addition &&  !g.edgeEnabled(edgeid)){
				removeEdgeFromMST(edgeid);
			}
		}
		prims();

		dbg_parents();
#ifndef NDEBUG
		assert(min_weight==dbg.forestWeight());
		assert(num_sets==dbg.numComponents());
#endif


		status.setMinimumSpanningTree(num_sets>1 ? INF: min_weight);

		//if(reportPolarity>-1){
		for(int i = 0;i<in_tree.size();i++){

			//Note: for the tree edge detector, polarity is effectively reversed.
			if(reportPolarity<1 && (!g.edgeEnabled(i) || in_tree[i]) ){
				status.inMinimumSpanningTree(i,true);
				assert(!g.edgeEnabled(i) || dbg.edgeInTree(i));
			}else if(reportPolarity>-1 && (g.edgeEnabled(i) && ! in_tree[i]) ){
				status.inMinimumSpanningTree(i,false);
				assert(!dbg.edgeInTree(i));
			}
		}
		assert(dbg_uptodate());

		last_modification=g.modifications;
		last_deletion = g.deletions;
		last_addition=g.additions;

		history_qhead=g.history.size();
		last_history_clear=g.historyclears;

		stats_full_update_time+=rtime(2)-startdupdatetime;;
	}
	vec<int> & getSpanningTree(){
		update();
		return mst;
	 }

	int getParent(int node){
		update();

		return parents[node];
	}
	 int getParentEdge(int node){
		 if(getParent(node)!=-1)
			 return parent_edges[node];
		 else
			 return -1;
	 }
	bool edgeInTree(int edgeid){
		update();
		return in_tree[edgeid];
	}
	bool dbg_mst(){

		return true;
	}


	int weight(){
		update();
		assert(dbg_uptodate());
		if(num_sets==1)
			return min_weight;
		else
			return INF;
	}

	int forestWeight(){
		update();
		assert(dbg_uptodate());
		return min_weight;
	}
	 int numComponents(){
		 update();
		 return num_sets;
	 }
	int getComponent(int node){
		update();
		return components[node];
	}
	int getRoot(int component=0){
		update();
		int c = 0;
		int nc = 0;
		while(component_member[c]==-1 || nc++<component){
			c++;
		}
		assert(component_member[c]>-1);
		return component_member[c];
		//return parents[component];
	}

	bool dbg_uptodate(){
#ifndef NDEBUG


		dbg_parents();
		//int n_components = 0;
		//check that each component has a unique root
		for(int c = 0;c<g.nodes;c++){
			if(component_member[c]>-1){
				//n_components++;
				int root = component_member[c];
				while(parents[root]>-1){
					root = parents[root];
				}

				for(int i = 0;i<g.nodes;i++){
					if(components[i]==c){
						//check that the root of i is root
						int p =i;
						while(parents[p]>-1){
							p = parents[p];
						}
						assert(p==root);
					}
				}

			}
		}

		assert(num_sets == g.nodes-empty_components.size());
		int sumweight = 0;
		in_tree.growTo(g.nEdgeIDs());
		for(int i = 0;i<g.nEdgeIDs();i++){
			if(in_tree[i]){
				sumweight+= g.getWeight(i);
			}
		}
		assert(sumweight ==min_weight);
#endif
		return true;
	};



/*
#ifndef NDEBUG
		int rootcount =0;
		for(int i = 0;i<parents.size();i++){
			if(parents[i]==-1)
				rootcount++;
		}
		assert(rootcount==1);
#endif
*/

};

#endif
