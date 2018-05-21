#include <cmath>
#include <cassert>

#include "dragontree.hpp"
#include "misc_utils.hpp"
#include "random_utils.hpp"

DragonTree::DragonTree( const Configuration &config, const string &name ) :
 Network( config, name )
{
	// create individual config objects for each sub-topology
	Configuration config_fly = config;
	Configuration config_fat = config;

	// customize the config object for FBfly
	config_fly.Assign("topology", "flatfly");
	config_fly.Assign("k", config.GetInt("fly_k"));
	config_fly.Assign("n", config.GetInt("fly_n"));
	config_fly.Assign("c", config.GetInt("fly_c"));

	// customize the config object for fattree
	config_fat.Assign("topology", "fattree");
	config_fat.Assign("k", config.GetInt("fat_k"));
	config_fat.Assign("n", config.GetInt("fat_n"));

	string rfunc = config.GetStr("routing_function");
	// get routing function
	if(rfunc == "deterministic") {
		routing = 0;
		config_fly.Assign("routing_function", "ran_min");	
		config_fat.Assign("routing_function", "nca");
	} else if(rfunc == "oblivious") {
		routing = 1;
		config_fly.Assign("routing_function", "ran_min");	
		config_fat.Assign("routing_function", "nca");
	} else if(rfunc == "adaptive") {
		routing = 2;
		config_fly.Assign("routing_function", "ugal");	
		config_fat.Assign("routing_function", "anca");
	} 

	assert(routing >= 0 && "[dragontree] unknown routing function");
	
	// register routing functions from individual subnets
	FlatFlyOnChip::RegisterRoutingFunctions();

	// instantiate the indivisual sub-topologies
	fly_net = new FlatFlyOnChip(config_fly, "fly_net");
	fattree_net = new FatTree(config_fat, "fattree_net");

	init(config);
}

void DragonTree::init(const Configuration &config)
{
	// get number of nodes from flatfly and fattree subnets
	int fly_num_nodes = fly_net->NumNodes();
	int fat_num_nodes = fattree_net->NumNodes();

	assert(fly_num_nodes == fat_num_nodes && "[dragontree] number of nodes is inconsistent");

	// record the number of nodes in the network
	num_nodes = fly_num_nodes;
	_nodes = fly_num_nodes;

	// initialize state variables
	num_vcs = config.GetInt("num_vcs");
	assert(num_vcs >= 0 && "[dragontree] incorrect number of vcs");
	fly_vc_buf.resize(_nodes);
	fat_vc_buf.resize(_nodes);
	for(int i = 0; i < _nodes; i++) {
		fly_vc_buf[i].resize(num_vcs);
		fat_vc_buf[i].resize(num_vcs);
	}
	active_lock_tbl = new int[num_vcs];
	for (int i = 0; i < num_vcs; i++) {
		active_lock_tbl[i] = -1;
	}
	node_net_map.resize(_nodes);
	credit_queues.resize(_nodes);
	load_fly.resize(_nodes);
	load_fat.resize(_nodes);
	for(int i = 0; i < _nodes; i++) {
		load_fly[i].resize(num_vcs);
		load_fat[i].resize(num_vcs);
	}
}

DragonTree::~DragonTree()
{
	delete fly_net;
	delete fattree_net;
	delete [] active_lock_tbl;
}

void DragonTree::WriteFlit(Flit* f, int source){
	cout << "[DEBUG} WriteFlit entry" << endl;
	
	// routing decisions are made from head flits
	if(f->head) {
		// deterministic routing
		if(routing == 0) {
			map_packet_to_net[f->pid] = fly_net;
		}

		// oblivious routing
		else if(routing == 1) {
			// flip a coin and choose one of the two subnets fairly
			if(RandomInt(1) == 0) {
				map_packet_to_net[f->pid] = fly_net;
			} else {
				map_packet_to_net[f->pid] = fattree_net;
			}
		}
		
		// adaptive routing
		else if(routing == 2) {
			int fly_local_load = load_fly[source][f->vc];
			int fat_local_load = load_fat[source][f->vc];
			if(fat_local_load > fly_local_load) {
				map_packet_to_net[f->pid] = fattree_net;
			} else {
				map_packet_to_net[f->pid] = fly_net;
			}
		}
	} 

	// all other flits follow the decision made by the head flit
	(map_packet_to_net[f->pid])->WriteFlit(f, source);

	// update local channel load estimates for adaptive routing
	if(routing == 2) {
		Network *net = map_packet_to_net[f->pid];
		if(net == fly_net) {
			load_fly[source][f->vc] -= 1;
		} else if(net == fattree_net) {
			load_fat[source][f->vc] -= 1;
		}

	}

	// remove network mapping if flit is tail flit
	if(f->tail) {
		map_packet_to_net.erase(f->pid);
	}


	cout << "[DEBUG} WriteFlit exit" << endl;
}

Credit *DragonTree::ReadCredit(int source)
{
	cout << "[DEBUG} ReadCredit entry" << endl;
	Credit *fly_credit = fly_net->ReadCredit(source);
	Credit *fat_credit = fattree_net->ReadCredit(source);

	Credit *c = (Credit *) NULL;
	if(fly_credit) {
		credit_queues[source].push_back(fly_credit);
		std::set<int>::iterator it;
		for(it = fly_credit->vc.begin(); it != fly_credit->vc.end(); it++) {
			load_fly[source][*it] += 1;
		}
	}
	if(fat_credit) {
		credit_queues[source].push_back(fat_credit);
		std::set<int>::iterator it;
		for(it = fat_credit->vc.begin(); it != fat_credit->vc.end(); it++) {
			load_fat[source][*it] += 1;
		}
	}
	
	if(!credit_queues[source].empty()) {
		c = credit_queues[source].front();
		credit_queues[source].pop_front();
	}

	cout << "[DEBUG} ReadFlit exit" << endl;
	return c;
}

void DragonTree::WriteCredit(Credit *c, int dest) 
{
	cout << "[DEBUG} WriteCredit entry" << endl;
	assert(node_net_map[dest] > 0);
	// if last ReadFlit was from flynet
	if(node_net_map[dest] == 1) {
		fly_net->WriteCredit(c, dest);
	}
	// if last ReadFlit was from fattree_net
	else if(node_net_map[dest] == 2) {
		fattree_net->WriteCredit(c, dest);
	}
	cout << "[DEBUG} WriteCredit exit" << endl;
}

Flit* DragonTree::ReadFlit(int dest) {

	// 1 = fly_net
	// 2 = fattree
	// 0 = null queue
	cout << "[DEBUG] ReadFlit entry" << endl;
	static int interleaver_curr = 1;

	Flit* fly_flit = fly_net->ReadFlit(dest);
	Flit* fat_flit = fattree_net->ReadFlit(dest);

	// first: enqueue flit
	if(fly_flit && fat_flit) {
		fly_vc_buf[dest][fly_flit->vc].push_back(fly_flit);
		fat_vc_buf[dest][fat_flit->vc].push_back(fat_flit);
	} else if (fly_flit) {
		fly_vc_buf[dest][fly_flit->vc].push_back(fly_flit);
	} else if (fat_flit) {
		fat_vc_buf[dest][fat_flit->vc].push_back(fat_flit);
	} else {
		null_queue.push_back((Flit *) NULL);
	}

	// second: readout
	// if the null queue is empty, go to the flynet queue
	if(interleaver_curr == 0 && null_queue.empty()) interleaver_curr = 1;
	// if flynet queue is empty, go to fattree queue
	if(interleaver_curr == 1) {
		bool all_empty = true;
		for(int i = 0; i < num_vcs; i++) {
			if(!fly_vc_buf[dest][i].empty()) {
				all_empty = false;
				break;
			}
		}
		if(all_empty) interleaver_curr = 2;
	}
	if(interleaver_curr == 2) {
		bool all_empty = true;
		for(int i = 0; i < num_vcs; i++) {
			if(!fat_vc_buf[dest][i].empty()) {
				all_empty = false;
				break;
			}
		}
		if(all_empty) {
			if(!null_queue.empty()) { interleaver_curr = 0; }
			else {
				bool flyq_empty = true;
				for(int i = 0; i < num_vcs; i++) {
					if(!fly_vc_buf[dest][i].empty()) {
						flyq_empty = false;
						break;
					}
				}
				if(flyq_empty) { interleaver_curr = 0; }
				else { interleaver_curr = 1; }
			}
		}
	}

	Flit *f = NULL;
	node_net_map[dest] = 0;

	if(interleaver_curr == 1) {
		for(int i = 0; i < num_vcs; i++) {
			if(!fly_vc_buf[dest][i].empty()) {
				f = fly_vc_buf[dest][i].front();
				fly_vc_buf[dest][i].pop_front();
				interleaver_curr = 2;
				break;
			}
		}
		if(f) node_net_map[dest] = 1;
	} else if(interleaver_curr == 2) {
		for(int i = 0; i < num_vcs; i++) {
			if(!fat_vc_buf[dest][i].empty()) {
				f = fat_vc_buf[dest][i].front();
				fat_vc_buf[dest][i].pop_front();
				interleaver_curr = 0;
				break;
			}
		}
		if(f) node_net_map[dest] = 2;
	} else {
		interleaver_curr = 1;
		if(!null_queue.empty()) { null_queue.pop_front(); }
	}

	cout << "[DEBUG] ReadFlit exit" << endl;
	return f;
}

void DragonTree::ReadInputs()
{
	fly_net->ReadInputs();
	fattree_net->ReadInputs();
}

void DragonTree::Evaluate()
{
	fly_net->Evaluate();
	fattree_net->Evaluate();
}

void DragonTree::WriteOutputs()
{
	fly_net->WriteOutputs();
	fattree_net->WriteOutputs();
}

void DragonTree::RegisterRoutingFunctions() {
	gRoutingFunctionMap["deterministic_dragontree"] = &dt_deterministic;
	gRoutingFunctionMap["oblivious_dragontree"] = &dt_oblivious;
	gRoutingFunctionMap["adaptive_dragontree"] = &dt_adaptive;
}


void dt_deterministic(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject)
{
	if(inject) {
		int inject_vc = RandomInt(gNumVCs-1);
		outputs->AddRange(-1, inject_vc, inject_vc);
		return;
	}
}

void dt_oblivious(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject)
{
	if(inject) {
		int inject_vc = RandomInt(gNumVCs-1);
		outputs->AddRange(-1, inject_vc, inject_vc);
		return;
	}
}

void dt_adaptive(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject)
{
	if(inject) {
		int inject_vc = RandomInt(gNumVCs-1);
		outputs->AddRange(-1, inject_vc, inject_vc);
		return;
	}
}
