import os.path
import sys
import random
import pandas as pd
import numpy as np
from sklearn.preprocessing import LabelEncoder
from sklearn.ensemble import RandomForestClassifier

pd.set_option('display.max_rows', 100)
pd.set_option('display.max_columns', 100)

print(sys.argv)
if len(sys.argv) != 6:
    print("Not enough information to run.")
    print("Need all of 'Path', 'Race', 'Player Name', 'Map', 'File In', and 'File Out'.")
    print("Exit.")
    sys.exit(0)

f = open("bwapi-data/write/test.txt", "w")
# 0. Get C++ arguments--------------------------------------------------------------------------------------
print(len(sys.argv))
print("Run a file: ", (sys.argv[0]))
print("We are at: ", os.getcwd())
f.write("We are at: " + os.getcwd() + "\n")

arguments = str(sys.argv)

opp_race = str(sys.argv[1])
print("Opp. Race: ", opp_race)
f.write("Opp. Race: " + opp_race + "\n")

opp_name = str(sys.argv[2])
print("Opp. Name: ", opp_name)
f.write("Opp. Name: " + opp_name + "\n")

opp_map = str(sys.argv[3])
print("Opp. Map: ", opp_map)
f.write("Opp. Map: " + opp_map + "\n")

file_in = str(os.path.join(os.getcwd(), sys.argv[4]))
print("File In: ", file_in)
f.write("File In: " + file_in + "\n")

file_out = str(os.path.join(os.getcwd(), sys.argv[5]))
print("File Out: ", file_out)
f.write("File Out: " + file_out + "\n")


# Def functions----------------------------------------------------------------------------------------------
def binary_convert(feature):
    df[feature] = df[feature].astype(float)
    for row in range(1, df.shape[0]):
        if df.at[row, feature] >= 1:
            df.at[row, feature] = 1
        else:
            df.at[row, feature] = 0


def generate_random_list():
    print(df_opening)

    ran_open = df_opening.shape[0]
    print(ran_open)
    opening_code_table = df_opening.index.tolist()
    print(opening_code_table)

    opening_index_table = []
    for ele in range(len(opening_code_table)):
        opening_index_table += [list(opening_code_table[ele])[0]]
    print(opening_index_table)
    np.random.choice(opening_index_table, 1)
    random_list = [round(np.random.rand(), 6), round(np.random.rand(), 6), round(np.random.rand(), 6),
                   round(random.uniform(0, 3), 6), round(np.random.rand(), 6), np.random.choice(opening_index_table, 1)] #np.random.randint(ran_open)]
    random_list.insert(3, round(1 - random_list[2], 6))
    return random_list


def generate_choose(dfg_test):
    df_test = []
    limit_attempt = 3
    cnt = 0
    continuing = True

    # Try until it predicts win or limit_attempts
    while continuing:
        random_list = generate_random_list()

        dfc_test = pd.DataFrame([random_list], columns=['gas_proportion', 'supply_ratio', 'avg_army',
                                                        'avg_econ', 'avg_tech', 'r', 'opening_code'])

        df_single_test = pd.concat([dfc_test, dfg_test], axis=1, sort=False)

        df_single_test_temp = df_single_test.astype(float)          # Convert to float
        df_single_test_temp['win'] = clf.predict(df_single_test)    # Predict and store
        single_test = df_single_test_temp.values.tolist()
        single_test = single_test[0]                                # Remove duplicate []

        # Check whether the result is win or loss
        if single_test[15] == 1.0:  # win case
            cnt += 1
            single_test.append(cnt)
            df_test += [single_test]
            continuing = False
        else:                       # lose case
            cnt += 1
            if cnt >= limit_attempt:
                single_test.append(cnt)
                df_test += [single_test]
                continuing = False

    print(df_test)
    # Decode the code of opp_features
    opening_code_table = df_opening.index.tolist()
    for i in range(len(opening_code_table)):
        opening_code_table[i] = list(opening_code_table[i])
        print(opening_code_table[i])
        print(opening_code_table[i][0])
        print(type(opening_code_table[i][0]))
        print(type(df_test[0][6]))
        if float(opening_code_table[i][0]) == df_test[0][6]:
            df_test[0][6] = opening_code_table[i][1]

    print(df_test)

    df_test[0][7] = opp_race
    df_test[0][8] = opp_name
    df_test[0][9] = opp_map

    print(df_test)

    df_test = pd.DataFrame(
        df_test, columns=['gas_proportion', 'supply_ratio', 'avg_army', 'avg_econ', 'avg_tech', 'r',
                          'opening', 'race', 'name', 'map', 'enemy_avg_army', 'enemy_avg_econ',
                          'enemy_avg_tech', 'detector_count', 'flyers', 'win', 'count'])
    return df_test


# -----------------------------------------------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------
# 1. Load Dataset--------------------------------------------------------------------------------------------
#path = os.getcwd()
file = file_in
print("Read a file from: ", file)
f.write("Read a file from: " + file)

col_ori = ['gas_proportion', 'supply_ratio', 'avg_army', 'avg_econ', 'avg_tech', 'r', 'race', 'win',
           'sdelay', 'mdelay', 'ldelay', 'name', 'map', 'enemy_avg_army', 'enemy_avg_econ', 'enemy_avg_tech',
           'opening', 'building', 'kills', 'raze', 'units', 'detector_count', 'flyers', 'duration']
df = pd.read_csv(file, names=col_ori)

if df.empty:
    print("No history records. Just exit.")
    sys.exit(0)

# 2. Clean Dataset-------------------------------------------------------------------------------------------
# Drop the header
df = df.drop(df.index[0])

print(df)

if df.empty:
    print("No history records. Just exit.")
    sys.exit(0)

if df.isnull().values.any():
    print("Missing values exist. Exit.")
    sys.exit(0)

# Convert 'detector_count' and 'flyers' feature (numerical number to binary number)
binary_convert('detector_count')
binary_convert('flyers')

# Encode numerical values
lb_make = LabelEncoder()
df["race_code"] = lb_make.fit_transform(df["race"])
df["name_code"] = lb_make.fit_transform(df["name"])
df["map_code"] = lb_make.fit_transform(df["map"])
df["opening_code"] = lb_make.fit_transform(df["opening"])

# Find and remove noise players.
df['win'] = df['win'].astype(int)
df_noise = df[["name", "win"]].groupby(["name"]).mean().rename(columns={'win': 'mean'})
df_noise = df_noise.loc[df_noise['mean'].isin([0, 1])]
noise_player = []
noise_player = df_noise.index.tolist()
df_cleaned = df.loc[~df['name'].isin(noise_player)]

# Create Code Table
df_race = df_cleaned[
    ['race_code', 'race', 'gas_proportion']].groupby(
    ['race_code', 'race']).count().rename(columns={'gas_proportion': 'count'})
print(df_race)

df_name = df_cleaned[
    ['name_code', 'name', 'gas_proportion']].groupby(
    ['name_code', 'name']).count().rename(columns={'gas_proportion': 'count'})
print(df_name)

df_map = df_cleaned[
    ['map_code', 'map', 'gas_proportion']].groupby(
    ['map_code', 'map']).count().rename(columns={'gas_proportion': 'count'})
print(df_map)

df_opening = df_cleaned[
    ['opening_code', 'opening', 'gas_proportion']].groupby(
    ['opening_code', 'opening']).count().rename(columns={'gas_proportion': 'count'})
print(df_opening)


# dfg = given
given = ['race_code', 'name_code', 'map_code', 'enemy_avg_army', 'enemy_avg_econ',
         'enemy_avg_tech', 'detector_count', 'flyers', 'win']
dfg = df_cleaned[given]

# dfc = choose
choose = ['gas_proportion', 'supply_ratio', 'avg_army', 'avg_econ', 'avg_tech', 'r', 'opening_code']
dfc = df_cleaned[choose]

# 3. Proper Dataset----------------------------------------------------------------------------------------
# Creat a train data set
df_train = pd.concat([dfc, dfg], axis=1, sort=False)
df_train = df_train.astype(float)
df_train = df_train.reset_index(drop=True)
df_temp = df_train.copy(deep=True)

enemy_avg_col = ['name_code', 'enemy_avg_army', 'enemy_avg_econ', 'enemy_avg_tech']
df_sum = df_train[enemy_avg_col].groupby(['name_code']).sum()
df_cnt = df_train[enemy_avg_col].groupby(['name_code']).count()

for i in range(df_train.shape[0]):
    if df_train.iloc[i][8] in df_sum.index.tolist():   # name match
        gp_army_sum = df_sum.at[df_train.iloc[i][8], 'enemy_avg_army']
        gp_army_cnt = df_cnt.at[df_train.iloc[i][8], 'enemy_avg_army']
        gp_tech_sum = df_sum.at[df_train.iloc[i][8], 'enemy_avg_tech']
        gp_tech_cnt = df_cnt.at[df_train.iloc[i][8], 'enemy_avg_tech']

    if df_cnt.at[df_train.iloc[i][8], 'enemy_avg_army'] != 1:
        df_temp.at[i, 'enemy_avg_army'] = (gp_army_sum - df_train.iloc[i][10]) / (gp_army_cnt - 1)
        df_temp.at[i, 'enemy_avg_econ'] = 1 - df_temp.at[i, 'enemy_avg_army']
        df_temp.at[i, 'enemy_avg_tech'] = (gp_tech_sum - df_train.iloc[i][12]) / (gp_tech_cnt - 1)
df_train = df_temp

# 4. Create a possible Train Dataset-----------------------------------------------------------------------
# Define train dataset
X_train = df_train.drop(['win'], axis=1)
y_train = df_train['win']

# Creat Random Forest Classifier
clf = RandomForestClassifier(n_estimators=500, max_depth=7, random_state=1234)
clf.fit(X_train, y_train)

# 5. Generate test dataset and predict the game result-----------------------------------------------------

# Testing Call arguments from C++

# Testing 0 record
#opp_name = "ABCDxyz"
#opp_map = "(4)Circuit Breaker.scx"
#opp_race = "Protoss"

# Testing 1 record
#opp_name = "CUBOT"
#opp_map = "(4)Circuit Breaker.scx"
#opp_race = "Zerg"

# Testing 2 record
#opp_name = "CUBOT"
#opp_map = "(4)Roadrunner.scx"
#opp_race = "Zerg"

#opp_race_code = "UNMATCHED"
#opp_name_code = "UNMATCHED"
#opp_map_code = "UNMATCHED"

# Find code of opp_features
race_code_table = df_race.index.tolist()
race_code_table.append((len(race_code_table), 'None'))
print(race_code_table)
for i in range(len(race_code_table)):
    race_code_table[i] = list(race_code_table[i])
    if race_code_table[i][1] == opp_race:
        opp_race_code = race_code_table[i][0]
if opp_race not in race_code_table[:][1]:
    opp_race_code = race_code_table[-1][0]
f.write(str(opp_race_code) + "\n")

name_code_table = df_name.index.tolist()
name_code_table.append((len(name_code_table), 'None'))
print(name_code_table)
for i in range(len(name_code_table)):
    name_code_table[i] = list(name_code_table[i])
    if name_code_table[i][1] == opp_name:
        opp_name_code = name_code_table[i][0]
if opp_name not in name_code_table[:][1]:
    opp_name_code = name_code_table[-1][0]
f.write(str(opp_name_code) + "\n")


map_code_table = df_map.index.tolist()
map_code_table.append((len(map_code_table), 'None'))
print(map_code_table)
for i in range(len(map_code_table)):
    map_code_table[i] = list(map_code_table[i])
    if map_code_table[i][1] == opp_map:
        opp_map_code = map_code_table[i][0]
if opp_map not in map_code_table[:][1]:
    opp_map_code = map_code_table[-1][0]
f.write(str(opp_map_code) + "\n")

# Find the records which match to opp_features
df_fit = df_train[(df_train['race_code'] == opp_race_code) &
                  (df_train['name_code'] == opp_name_code) &
                  (df_train['map_code'] == opp_map_code)]
df_fit = df_fit.reset_index(drop=True)

# No record, 1 record, and 2+ records
if df_fit.shape[0] == 0:                        # 0 Record

    """
    det_max = df_train['detector_count'].max()
    fly_max = df_train['flyers'].max()
    dfg_test_temp = df_train[['race_code', 'name_code', 'map_code',
                              'enemy_avg_army', 'enemy_avg_econ', 'enemy_avg_tech'
                              ]].groupby(['race_code']).mean()
    r_list = dfg_test_temp.index.values.tolist()
    if opp_race_code not in r_list:
        
    
    if (opp_race_code == race_code_table[-1][0]
            and opp_name_code == name_code_table[-1][0]
            and opp_map_code != map_code_table[-1][0]):
        det_max = df_train['detector_count'].max()
        fly_max = df_train['flyers'].max()
        dfg_test_temp = df_train[['race_code', 'name_code', 'map_code',
                                  'enemy_avg_army', 'enemy_avg_econ', 'enemy_avg_tech'
                                  ]].groupby(['race_code', 'name_code']).mean()

    elif (opp_race_code == race_code_table[-1][0]
            and opp_name_code != name_code_table[-1][0]
            and opp_map_code == map_code_table[-1][0]):
        print(1)
    elif (opp_race_code != race_code_table[-1][0]
            and opp_name_code == name_code_table[-1][0]
            and opp_map_code == map_code_table[-1][0]):
        print(1)
    elif (opp_race_code != race_code_table[-1][0]
            and opp_name_code != name_code_table[-1][0]
            and opp_map_code == map_code_table[-1][0]):
        print(1)
    elif (opp_race_code == race_code_table[-1][0]
            and opp_name_code != name_code_table[-1][0]
            and opp_map_code != map_code_table[-1][0]):
        print(1)
    elif (opp_race_code != race_code_table[-1][0]
            and opp_name_code == name_code_table[-1][0]
            and opp_map_code != map_code_table[-1][0]):
        print(1)
    """
    dfg_test = pd.DataFrame([
        (opp_race_code, opp_name_code, opp_map_code, 0, 0, 0, 0, 0)],
        columns=given[:-1], index=[0])          # Set given features
    df_final = generate_choose(dfg_test)        # Set choosing features

else:
    if df_fit.shape[0] == 1:                    # 1 Record
        dfg_test = df_fit[given[:-1]]           # Set given features
        df_final = generate_choose(dfg_test)    # Set choosing features

    else:                                       # 2 Records
        dfg_test_temp = df_fit[given[:-1]]      # Set given features

        # Find max for binary feature and mean for numerical features
        det_max = dfg_test_temp['detector_count'].max()
        fly_max = dfg_test_temp['flyers'].max()
        dfg_test_cleaned = dfg_test_temp.mean()

        dfg_test = pd.DataFrame([(dfg_test_cleaned[0], dfg_test_cleaned[1], dfg_test_cleaned[2],
                                  dfg_test_cleaned[3], dfg_test_cleaned[4], dfg_test_cleaned[5],
                                  det_max, fly_max)], columns=given[:-1], index=[0])

        df_final = generate_choose(dfg_test)    # Set choosing features


print(df_final)

# Generate result
df_final.to_csv(file_out, index=False)