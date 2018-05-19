#include <cmath>

#include "dragontree.hpp"
#include "misc_utils.hpp"

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
	config_fly.Assign("routing_function", "ran_min");

	// customize the config object for fattree
	config_fat.Assign("topology", "fattree");
	config_fat.Assign("k", config.GetInt("fat_k"));
	config_fat.Assign("n", config.GetInt("fat_n"));
	config_fat.Assign("routing_function", "nca");

	// instantiate the indivisual sub-topologies
	FlatFlyOnChip::RegisterRoutingFunctions();
	DragonTree::RegisterRoutingFunctions();
	fly_net = new FlatFlyOnChip(config_fly, "fly_net");
	fattree_net = new FatTree(config_fat, "fattree_net");

	_ComputeSize(config);
	_Alloc();
	_BuildNet(config);
}

void DragonTree::_ComputeSize( const Configuration &config )
{
	int dk, dn;
	dk = config.GetInt("fat_k");
	dn = config.GetInt("fat_n");

	_nodes = powi(dk, dn);
	_channels = (2*dk * powi( dk , dn-1 ))*(dn-1);
	_size = dn * powi( dk , dn - 1 );
	cout << "dragontree _ComputeSize" << endl;
}

void DragonTree::_BuildNet( const Configuration &config )
{
	cout << "dragontree _BuildNet" << endl;
}

DragonTree::~DragonTree()
{
	delete fly_net;
	delete fattree_net;
}

void DragonTree::RegisterRoutingFunctions() {
	gRoutingFunctionMap["deterministic_dragontree"] = &dt_deterministic;
}


void dt_deterministic(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject)
{
	min_flatfly(r, f, in_channel, outputs, inject);
}