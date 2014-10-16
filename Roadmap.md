# DRAFT

This is our general plan for developing **stellard**. Until December we are mostly working on changing the core data structures and simplifying the protocol. The goal is to get much higher performance and scalability and make it a tractable task for people to build alternate implementations.

Times, priority, and tasks are all subject to change as the network evolves, we grow the team, and receive more or less contributions from the community. We use github issues for all the near term [items we are working on.](https://github.com/stellar/stellard/issues)

### Our Plan

#### Performance & Scalability Refactor
1. Redesign node store/ledger hashing/communication protocol. (september)
2. Do apply only once. (september)
3. Replay stream of transactions to recreate a given ledger. (october)
4. Performance testing (october)
5. Decouple Transactions from nodestore (october)
6. Move Ledger objects into SQL. (october)
7. Inflation using the SQL store (october)
8. History as stream of transactions with ledger checkpoints. (october)
9. Change how nodes fetch the history from Network. (november)
10. Make SLEs actual classes (november)
11. Make offer matching and other things use the SQL DB rather the SLEs. (november) 
12. Temporal buckets for ledger objects. (december)
13. SQL backend for postgres or someother DB (december)
14. Clean up dead nodestore code. (december)


#### New features
1. Merge accounts. This will allow people to clean up accounts they are no longer using and recover the reserve balance.  (september)
2. Pull changes from rippled. This is an ongoing process but we will spend some time looking through the rippled commits since we diverged and see what makes sense to pull in. (september)
3. Improve deployment pipeline. (october)
4. Merge various transaction memo fields into one. (december)
5. Clean up API. *please [let us know](https://github.com/stellar/stellar-protocol/issues) anything you would like to see changed* (december)
6. New consensus algorithm. (2015)
7. Ephemeral messaging. (2015)
8. Multi-sig. (2015)
9. Design Scripting/contracts. (2015)
10. Start implementing Private transactions. (2015)
 

### How you can help
- Submit issues
- [Fix issues.](https://github.com/stellar/stellard/labels/contribute) We mark the best ones to get started contributing with the `contribute` label.
- Write tests!
