# CUNYAIModule

Description:
CUNYBot (City University of New York) is a C++ bot that plays a full game of Starcraft:BW as the Zerg race. Its central design feature is a focus on historical economic models.  In particular, it focuses on the capital/labor ratio (*k*), and technology/labor ratios (*t*), where it responds to percieved opponent's choice of *k*.  The resulting emergent behavior can be characterized as a greedy tit-for-tat style response. Its current win rate on BASIL is about 50%.

Link to complete bot description here: https://ieeexplore.ieee.org/document/8490444

Other measures of bot performance are available at: https://www.basil-ladder.net/ranking.html#Bryan%20Weber

Recent Development Notes: 

(1/23/2021)  You can now play against this bot as a human on SCHNAIL: https://schnail.com/#/home

(11/17/2020) We now use grids for some portions of combat, 256x256 arrays.  We also use 2 similar phases of "reservation". The first is the already existing build order system (not BOSS) executes preselected plans. It passes this plan to a short term reservation manager, so long as it does not collide with an existing reserved resource (larva, min, gas, supply).  All macro actions (morph, build, research) are now reserved through this short term reservation manager, which allows for us to "multitask"- such as building lings with excess minerals while still saving for gas-heavy purchases like mutas.

(9/29/2020) Operationally, we have backed out of python and pybind11. There are various operating theaters for bots and it is challenging to find one method execution that works for all of them.

(2/19/2020)  This is a huge update. The bot now uses Python via pybind11.  This permits (easy) access to the python libraries, but greatly increases the size of the bot. Installation now requires manually downloading python36.dll and using pip to install the requisite dependancies.  An option to compile without these dependancies is still available.

(9/26/2018) Incorperated FAP fully, BWEM is now used for pathing and movement, and BWEB is now used for building, including some walls at the natural. Several functions have been deeply refactored (particularly workers) and combat has been deeply streamlined for student use. The bot now makes some estimates of what opponents may have behind the fog of war, and better responds to observed technical investments.

(9/6/2018) Current drafts of CUNYBot are responding to opponent's choice of *t* as well, and the overall observed behavior remains a tit-for-tat style.  Technology has been closely integrated and better improved in current versions, but have not yet released these to SSCAIT. Tasks at hand are generally production choices, and reducing waste in combat.  To that end, FAP has been added as a combat simulator. It also serves as an agnostic method of choosing which units to produce, with some caveats.
