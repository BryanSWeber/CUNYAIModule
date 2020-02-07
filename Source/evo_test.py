import cma
import pandas as pd
from pathlib import Path

print("We're in.")
print(str(Path().resolve().parent.parent))
data = pd.read_csv(Path().resolve().parent.parent.joinpath("write").joinpath("UnitWeights.txt"))
print(data)
utility = data.Score
print(utility)
data = data.drop("Score", axis = 1)
solutions = data 
print(solutions)

print("set options")
cma.CMAOptions().set({'BoundaryHandler': cma.BoundPenalty, 'bounds': [-1, 1]})

print("initialize CMAES")
es = cma.CMAEvolutionStrategy(solutions.iloc[0], 0.5)

try:
    answers = [];
    for util in utility:
        answers.append(util)
    print("Asking")
    es.ask()

    print("Telling")
    es.tell(solutions, answers)
    print(es.disp())

    print("Asking")
    unit_weights = es.ask()[0]
except ValueError:
    print("Not enough examples yet") #I need more than 206 examples before things start improving.
    unit_weights = es.ask()[0]
    
print(es.result_pretty())
print(unit_weights)