# CUNYAIModule

Description:
CUNYBot (City University of New York) is a C++ bot that plays a full game of Starcraft:BW as the Zerg race. Its central design feature is a focus on historical economic models.  In particular, it focuses on the capital/labor ratio (*k*), and technology/labor ratios (*t*), where it responds to percieved opponent's choice of *k*.  The resulting emergent behavior can be characterized as a greedy tit-for-tat style response. 

Link to complete bot description here: (p 417) 
https://project.dke.maastrichtuniversity.nl/games/files/proceedings-CIG2018.pdf

Recent Development Notes: 

(9-6-2018) Current drafts of CUNYBot are responding to opponent's choice of *t* as well, but the overall observed behavior remains a tit-for-tat style.  Technology has been closely integrated and better improved in current versions, but have not yet released these to SSCAIT. Tasks at hand are generally production choices, and reducing waste in combat.  To that end, FAP has been added as a combat simulator. It also serves as an agnostic method of choosing which units to produce, with some caveats.
