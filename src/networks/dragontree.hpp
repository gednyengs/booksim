#ifndef _DragonTree_HPP_
#define _DragonTree_HPP_

#include "network.hpp"
#include "routefunc.hpp"
#include "flatfly_onchip.hpp"
#include "fattree.hpp"

class DragonTree : public Network {
	FlatFlyOnChip *fly_net;
	FatTree *fattree_net;
		
	void _ComputeSize( const Configuration &config );
	void _BuildNet( const Configuration &config );
public:
	DragonTree( const Configuration &config, const string &name );
	~DragonTree();

	static void RegisterRoutingFunctions();
};

void dt_adaptive(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);
void dt_deterministic(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);
void dt_oblivious(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);

#endif /* _DragonTree_HPP_ */
