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
	//DragonTree::RegisterRoutingFunctions();

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
	fly_vc_buf.resize(num_vcs);
	fat_vc_buf.resize(num_vcs);
	active_lock_tbl = new int[num_vcs];
	for (int i = 0; i < num_vcs; i++) {
		active_lock_tbl[i] = -1;
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
	cout << "routing fn = " << routing << endl;
	// static int dest_net = 0;
	if (f->id == 18 ){
		cout << "[WriteFlit ] f(18) dest " << f->dest << endl;
	}
	if (!f->head) {
		cout << "flit id " << f->id << " is not head" << endl;
		Network *net = map_packet_to_net[f->pid];
		net->WriteFlit(f, source);
		if (routing != 0) {
			if(net == fly_net) {
				cout << "We should not be here 1" << endl;
				fattree_net->WriteFlit(NULL, source);
			} else {
				cout << "We should not be here 2 " << endl;
				fly_net->WriteFlit(NULL, source);
			}
		}
	} else {
		cout << "flit id " << f->id << " is head" << endl;
		assert(routing == 0);
		if (routing == 0) {
			//if(dest_net == 0) {
				fly_net->WriteFlit(f, source);
				// fattree_net->WriteFlit(NULL, source);
				map_packet_to_net[f->pid] = fly_net;
			//} else {
			//	fattree_net->WriteFlit(f, source);
			//	fly_net->WriteFlit(NULL, source);
			//;	map_packet_to_net[f->pid] = fattree_net;
			//}
			//dest_net = (dest_net + 1) % 2;

			//fattree_net->WriteFlit(NULL, source);
			
		} else if (routing == 1) {
			if (RandomInt(1) == 0) {
				fly_net->WriteFlit(f, source);
				fattree_net->WriteFlit(NULL, source);
				map_packet_to_net[f->pid] = fly_net;
			} else {
				fattree_net->WriteFlit(f,source);
				fly_net->WriteFlit(NULL, source);
				map_packet_to_net[f->pid] = fattree_net;
			}
		} else {
			// routing == 2 
		}
	}
	if (f->tail) {
		map_packet_to_net.erase(f->pid);
	}
	cout << "[DEBUG} WriteFlit exit" << endl;
}

Credit *DragonTree::ReadCredit(int source)
{
	cout << "[DEBUG} ReadCredit entry" << endl;
	Credit *fly_credit = fly_net->ReadCredit(source);
	// Credit *fat_credit = fattree_net->ReadCredit(source);

	credit_queue.emplace_back(fly_credit);
	// credit_queue.emplace_back(fat_credit);

	Credit *c = credit_queue.front();
	credit_queue.pop_front();
	cout << "[DEBUG} ReadFlit exit" << endl;
	return c;
}

void DragonTree::WriteCredit(Credit *c, int dest) 
{
	cout << "[DEBUG} WriteCredit entry" << endl;
	if(!credit_destinations.empty()) {
		int dest_net = credit_destinations.front();
		credit_destinations.pop_front();
		assert(dest_net == 1);
		if(dest_net == 1) {
			fly_net->WriteCredit(c, dest);
			// fattree_net->WriteCredit(NULL, dest);
		} else if(dest_net == 2) {
			fattree_net->WriteCredit(c, dest);
			fly_net->WriteCredit(NULL, dest);
		}
	}
	cout << "[DEBUG} WriteCredit exit" << endl;

}

Flit* DragonTree::ReadFlit(int dest) {

	// 1 = fly_net
	// 2 = fattree
	// 3 = null queue
	cout << "[DEBUG] ReadFlit entry" << endl;
	static int interleaver_curr = 1;

	Flit* fattree_flit = fattree_net->ReadFlit(dest);
	Flit* fly_flit = fly_net->ReadFlit(dest);

	// first: enqueue flits
	if (fattree_flit != NULL && fly_flit != NULL) {
		cout << "[DEBUG}  fattree_flit != NULL && fly_flit != NULL" << endl;
		fat_vc_buf[fattree_flit->vc].emplace_back(fattree_flit);
		fly_vc_buf[fly_flit->vc].emplace_back(fly_flit);	
	} else if (fattree_flit != NULL) {
		cout << "[DEBUG}  fattree_flit != NULL && fly_flit == NULL" << endl;
		fat_vc_buf[fattree_flit->vc].emplace_back(fattree_flit);
	} else if (fly_flit != NULL) {
		cout << "[DEBUG}  fattree_flit == NULL && fly_flit != NULL" << endl;
		fly_vc_buf[fly_flit->vc].emplace_back(fly_flit);
	} else {
		cout << "[DEBUG}  fattree_flit == NULL && fly_flit == NULL" << endl;
		null_queue.emplace_back((Flit *) NULL);
	}

	

	// second: find flit to send to trafficmanager
	// 1 = fly_net
	// 2 = fat_tree
	// 0 = none
	Flit* ret = NULL;
	int out_net = -1;
	
	cout << "interleaver_curr @ entry = " << interleaver_curr << endl;
	for (int j = 0; j < 3; j++) {
		if (interleaver_curr == 1) {
			cout << "interleaver_curr = 1" << endl;
			for (int i =0; i < num_vcs; i++) {
				if(!fly_vc_buf[i].empty()){
					if (active_lock_tbl[i] == 1) {
						ret = fly_vc_buf[i].front();
						fly_vc_buf[i].pop_front();
						if (ret->tail) {
							active_lock_tbl[i] = -1;
						}
						break;
					} else if (active_lock_tbl[i] == -1) {
						active_lock_tbl[i] = 1;
						ret = fly_vc_buf[i].front();
						fly_vc_buf[i].pop_front();
						if (ret->tail) {
							active_lock_tbl[i] = -1;
						}
						break; 
					}
				}
			}
			if (ret) {
				out_net = interleaver_curr;
				interleaver_curr = (interleaver_curr + 1) % 3;
				cout << "interleaver_curr = 1 break" << endl;
				break;
			}
		}
		if (interleaver_curr == 2) {
			cout << "interleaver_curr = 2" << endl;
			for (int i =0; i < num_vcs; i++) {
				if(!fat_vc_buf[i].empty()){
					if (active_lock_tbl[i] == 2) {
						ret = fat_vc_buf[i].front();
						fat_vc_buf[i].pop_front();
						if (ret->tail) {
							active_lock_tbl[i] = -1;
						}
						break;
					} else if (active_lock_tbl[i] == -1) {
						active_lock_tbl[i] = 2;
						ret = fat_vc_buf[i].front();
						fat_vc_buf[i].pop_front();
						if (ret->tail) {
							active_lock_tbl[i] = -1;
						}
						break; 
					}
				}
			}
			if (ret) {
				out_net = interleaver_curr;
				interleaver_curr = (interleaver_curr + 1) % 3;
				cout << "interleaver_curr = 2 break" << endl;
				break;
			}
		}
		if (interleaver_curr == 0) {
			cout << "interleaver_curr = 0" << endl;
			if (!null_queue.empty()){
				ret = NULL;
				out_net = interleaver_curr;
				interleaver_curr = (interleaver_curr + 1) % 3;
				cout << "interleaver_curr = 0 break" << endl;
				break;
			}
		}
		interleaver_curr = (interleaver_curr + 1) % 3;
	}
		
	// record which subnet we read from
	if(out_net >= 1) {
		credit_destinations.emplace_back(out_net);
	}	

	// FOR DEBUGGING
	cout << "[DEBUG} ReadFlit exit" << endl;
	return ret;
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