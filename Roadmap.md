This is our general plan for developing stellard. Until december we are mostly working on changing the core data structures and simplifying the protocol. This will allow us to get much higher performance and scalability and should make it a tractable task for people to build alternate implementations.

Times, priority, and tasks are all subject to change as the network evolves, we grow the team, and receive more or less contributions from the community. We use github issues for all the near term [items we are working on.](https://github.com/stellar/stellard/issues)

### Our plan
1. Merge accounts. This will allow people to clean up accounts they are no longer using and recover the reserve balance.  (september)
2. Redesign node store/ledger hashing/communication protocol. (september)
3. Pull changes from rippled. This is an ongoing process but we will spend some time looking through the rippled commits since we diverged and see what makes sense to pull in. (october)
4. Clean up API. *please [let us know](https://github.com/stellar/stellar-protocol/issues) anything you would like to see changed* (october)
5. Improve deployment pipeline. (october)
6. New way of keeping track of ledger history. (october)
7. Change how nodes fetch the history. (october)
8. Move Ledger objects into SQL (october)
9. New way of determining the ledger hash. (november)
10. Drop node store (november)
11. New consensus algorithm. (november)
12. Multi-sig (november)
13. Ephemeral messaging (november)
14. Scripting/contracts (december)
15. Private transactions (december)


### How you can help
- Submit issues
- [Fix issues.](https://github.com/stellar/stellard/labels/help%20wanted) We mark the best ones to get started contributing with the 'help wanted' label.
- Write tests!
