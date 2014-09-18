# DRAFT

This is our general plan for developing **stellard**. Until December we are mostly working on changing the core data structures and simplifying the protocol. The goal is to get much higher performance and scalability and make it a tractable task for people to build alternate implementations.

Times, priority, and tasks are all subject to change as the network evolves, we grow the team, and receive more or less contributions from the community. We use github issues for all the near term [items we are working on.](https://github.com/stellar/stellard/issues)

### Our Plan

#### Performance & Scalability Refactor
1. Redesign node store/ledger hashing/communication protocol. (september)
2. Replay stream of transactions to recreate a given ledger. (september)
3. History as stream of transactions with ledger checkpoints. (october)
4. Change how nodes fetch the history from Network. (october)
5. Move Ledger objects into SQL. (october)
6. Do offer matching/inflation and other lookups in the SQL DB. (october) 
7. Temporal buckets for ledger objects. (november)
8. New way of determining the ledger hash. (november)
9. Drop node store. (november)


#### New features
1. Merge accounts. This will allow people to clean up accounts they are no longer using and recover the reserve balance.  (september)
2. Pull changes from rippled. This is an ongoing process but we will spend some time looking through the rippled commits since we diverged and see what makes sense to pull in. (october)
3. Improve deployment pipeline. (october)
4. Merge various transaction memo fields into one. (october)
5. Clean up API. *please [let us know](https://github.com/stellar/stellar-protocol/issues) anything you would like to see changed* (october)
6. New consensus algorithm. (december)
7. Ephemeral messaging. (december)
8. Multi-sig. (december)
9. Design Scripting/contracts. (december)
10. Start implementing Private transactions. (january)
 

### How you can help
- Submit issues
- [Fix issues.](https://github.com/stellar/stellard/labels/help%20wanted) We mark the best ones to get started contributing with the `help wanted` label.
- Write tests!
