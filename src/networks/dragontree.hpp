#ifndef _DragonTree_HPP_
#define _DragonTree_HPP_

#include <iostream>
#include <deque>
#include <vector>

#include "network.hpp"
#include "routefunc.hpp"
#include "flatfly_onchip.hpp"
#include "fattree.hpp"

using namespace std;

class DragonTree : public Network {

	// subnetworks
	FlatFlyOnChip *fly_net;
	FatTree *fattree_net;
	
	// global topology info
	int num_nodes;
	int routing; // 0 = deterministic, 1 = oblivious, 2 = adaptive

	// state variables
	vector<deque<Flit *> > fly_vc_buf, fat_vc_buf;
	deque<int> credit_destinations;
	deque<Credit *> credit_queue;
	deque<Flit *> null_queue;
	int num_vcs;
	int *active_lock_tbl;

	std::map<int, Network*> map_packet_to_net;

	// helper functions
	void init( const Configuration &config );

public:
	DragonTree( const Configuration &config, const string &name );
	~DragonTree();

	// Network functions override
	inline int NumNodes( ) const { return num_nodes; }
	void OutChannelFault( int r, int c, bool fault = true ) {}
	void ReadInputs( );
	void Evaluate( );
	void WriteOutputs( );

	void Display( ostream & os = cout ) const {}
  	void DumpChannelMap( ostream & os = cout, string const & prefix = "" ) const {}
  	void DumpNodeMap( ostream & os = cout, string const & prefix = "" ) const {}

	void WriteFlit( Flit *f, int source );
  	Flit *ReadFlit( int dest );
  	void WriteCredit( Credit *c, int dest );
  	Credit *ReadCredit( int source );

  	int NumChannels() const {return _channels;}
	const vector<FlitChannel *> & GetInject() {return fly_net->GetInject();}
	FlitChannel * GetInject(int index) {return fly_net->GetInject(index);}
	const vector<CreditChannel *> & GetInjectCred() {return fly_net->GetInjectCred();}
	CreditChannel * GetInjectCred(int index) {return fly_net->GetInjectCred(index);}
	const vector<FlitChannel *> & GetEject(){return fly_net->GetEject();}
	FlitChannel * GetEject(int index) {return fly_net->GetEject(index);}
	const vector<CreditChannel *> & GetEjectCred(){return fly_net->GetEjectCred();}
	CreditChannel * GetEjectCred(int index) {return fly_net->GetEjectCred(index);}
	const vector<FlitChannel *> & GetChannels(){return fly_net->GetChannels();}
	const vector<CreditChannel *> & GetChannelsCred(){return fly_net->GetChannelsCred();}
	const vector<Router *> & GetRouters(){return fly_net->GetRouters();}
	Router * GetRouter(int index) {return fly_net->GetRouter(index);}
	int NumRouters() const {return fly_net->NumRouters();}

	static void RegisterRoutingFunctions();
	void _ComputeSize(const Configuration &config) {}
	void _BuildNet(const Configuration &config) {}
};

void dt_adaptive(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);
void dt_deterministic(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);
void dt_oblivious(const Router *r, const Flit *f, int in_channel,
		OutputSet *outputs, bool inject);

#endif /* _DragonTree_HPP_ */
