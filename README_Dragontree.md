# Notes on code changes needed to support the dragontree topology
In this implementation, the dragontree topology instantiates two sub-topologies and manages them.
Those sub-topologies are *fattree (FatTree)* and *flattened butterfly (FlatFlyOnChip)*.
However, a few changes are needed in order to make the dragontree topology to work properly.

## ReadFlit(n)
