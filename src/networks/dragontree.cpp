

#include "dragontree.hpp"

DragonTree::DragonTree( const Configuration &config, const string &name ) :
 Network( config, name )
{
	fly_net = new FlatFlyOnChip(..);
	fattree_net = new FatTree(...);
}

DragonTree::~DragonTree()
{
	delete fly_net;
	delete fattree_net;
}
