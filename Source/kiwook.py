import os.path
import random
import pandas as pd
import numpy as np
from sklearn.preprocessing import LabelEncoder
from sklearn.ensemble import RandomForestClassifier

pd.set_option('display.max_rows', 100)
pd.set_option('display.max_columns', 100)

#if len(local) != 6:
#    print("Not enough information to run.")
#    print("Need all of 'Path', 'Race', 'Player Name', 'Map', 'File In', and 'File Out'.")
#    print("Exit.")

# 0. Get C++ arguments--------------------------------------------------------------------------------------
print("We are at: " + os.getcwd() + "\n")
print("Our parent is " + os.path.normpath(os.path.join(os.getcwd(), os.pardir)) + "\n")

opp_race = e_race
print("Opp. Race: " + opp_race + "\n")

opp_name = e_name
print("Opp. Name: " + opp_name + "\n")

opp_map = e_map
print("Opp. Map: " + opp_map + "\n")

file_in = str(os.path.join(os.path.abspath('..'), in_file))
print("File In: " + file_in + "\n")

abort_code_t0 = False;

# Def functions----------------------------------------------------------------------------------------------
def binary_convert(df_in, feature):
    df_in[feature] = df_in[feature].astype(float)
    for row in range(1, df_in.shape[0]):
        if df_in.at[row, feature] >= 1:
            df_in.at[row, feature] = 1
        else:
            df_in.at[row, feature] = 0
    return df_in

def generate_choose(df_opening_in, dfg_test_in):
    import pandas as pd
    import numpy as np
    df_test = []
    limit_attempt = 3
    cnt = 0
    continuing = True
    print("Made it inside Generate Choose")
    # Try until it predicts win or limit_attempts
    while continuing:
    
        print("Attempting to generate a random guess:")
        print(df_opening_in)
        ran_open = df_opening_in.shape[0]
        print(ran_open)
        opening_code_table = df_opening_in.index.tolist()
        print(opening_code_table)
        opening_index_table = []
        for ele in range(len(opening_code_table)):
            opening_index_table += [list(opening_code_table[ele])[0]]
        print(opening_index_table)
        print(np.pi)
        print(np.random.choice(opening_index_table))
        opn = np.random.choice(opening_index_table, 1)[0]
        opn = int(opn)
        print(opn)
        print(round(np.random.rand(), 6))
        random_list = [round(np.random.rand(), 6), round(np.random.rand(), 6), round(np.random.rand(), 6),round(np.random.uniform(0, 3), 6), round(np.random.rand(), 6), opn] 
        print(random_list)
        random_list.insert(3, round(1 - random_list[2], 6))
        current_guess = random_list
        print("Guess Made")
        print(current_guess)
        
        dfc_test = pd.DataFrame([current_guess], columns=['gas_proportion', 'supply_ratio', 'avg_army', 'avg_econ', 'avg_tech', 'r', 'opening_code'])
        df_single_test = pd.concat([dfc_test, dfg_test_in], axis=1, sort=False)

        print("Testing the following:")
        print(df_single_test)
        
        df_single_test_temp = df_single_test.astype(float)          # Convert to float
        df_single_test_temp['win'] = clf.predict(df_single_test)    # Predict and store
        print(df_single_test_temp['win'])
        single_test = df_single_test_temp.values.tolist()
        print(single_test)
        single_test = single_test[0]                                # Remove duplicate []
        print(single_test)
        
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
    opening_code_table = df_opening_in.index.tolist()
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
print("Reading a file from: " + file)

col_ori = ['gas_proportion', 'supply_ratio', 'avg_army', 'avg_econ', 'avg_tech', 'r', 'race', 'win',
           'sdelay', 'mdelay', 'ldelay', 'name', 'map', 'enemy_avg_army', 'enemy_avg_econ', 'enemy_avg_tech',
           'opening', 'building', 'kills', 'raze', 'units', 'detector_count', 'flyers', 'duration']
df = pd.read_csv(file, names=col_ori)

print("Finished reading a file from: " + file)

if df.empty:
    print("No history records. Just exit.")
    abort_code_t0 = True;
    print(abort_code_t0)
else: 
    print("We have history records")

if df.isnull().values.any():
    print("Missing values exist. Exit.")
    abort_code_t0 = True;
    print(abort_code_t0)
else: 
    print("No missing values")

if abort_code_t0 == False:
    # 2. Clean Dataset-------------------------------------------------------------------------------------------
    # Drop the header
    df = df.drop(df.index[0])

    # Convert 'detector_count' and 'flyers' feature (numerical number to binary number)
    df = binary_convert(df,'detector_count')
    df = binary_convert(df,'flyers')

    print("Binary Conversion Complete")

    # Encode numerical values
    lb_make = LabelEncoder()
    df["race_code"] = lb_make.fit_transform(df["race"])
    df["name_code"] = lb_make.fit_transform(df["name"])
    df["map_code"] = lb_make.fit_transform(df["map"])
    df["opening_code"] = lb_make.fit_transform(df["opening"])

    print("Encoding Complete")

    # Find and remove noise players.
    df['win'] = df['win'].astype(int)
    df_noise = df[["name", "win"]].groupby(["name"]).mean().rename(columns={'win': 'mean'})
    df_noise = df_noise.loc[df_noise['mean'].isin([0, 1])]
    noise_player = []
    noise_player = df_noise.index.tolist()
    df_cleaned = df.loc[~df['name'].isin(noise_player)]

    print("Noise Removed")

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

    print("Finished cleaning the data")

    # 3. Proper Dataset----------------------------------------------------------------------------------------
    # Create a train data set
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

    print("Finished creating training data")
    print(df_train)
    
    if df_train.empty or df_train.shape[0] <= 1:
        print("Training File is empty.")
        abort_code_t0 = True;
        print(abort_code_t0)
    else: 
        print("Training File is not empty.")

if abort_code_t0 == False:
    # 4. Create a possible Train Dataset-----------------------------------------------------------------------
    # Define train dataset
    X_train = df_train.drop(['win'], axis=1)
    y_train = df_train['win']

    # Creat Random Forest Classifier
    clf = RandomForestClassifier(n_estimators=500, max_depth=7, random_state=1234)
    clf.fit(X_train, y_train)

    print("Finished fitting RF")
    # 5. Generate test dataset and predict the game result-----------------------------------------------------

    # Find code of opp_features
    race_code_table = df_race.index.tolist()
    race_code_table.append((len(race_code_table), 'None'))
    print(race_code_table)
    opp_race_code = race_code_table[-1][0]
    for i in range(len(race_code_table)):
        race_code_table[i] = list(race_code_table[i])
        if race_code_table[i][1] == opp_race:
            opp_race_code = race_code_table[i][0]
    print("Opponent Race Code is: " + str(opp_race_code) + "\n")

    name_code_table = df_name.index.tolist()
    name_code_table.append((len(name_code_table), 'None'))
    print(name_code_table)
    opp_name_code = name_code_table[-1][0]
    for i in range(len(name_code_table)):
        name_code_table[i] = list(name_code_table[i])
        if name_code_table[i][1] == opp_name:
            opp_name_code = name_code_table[i][0]
    print("Opponent Name Code is: " + str(opp_name_code) + "\n")


    map_code_table = df_map.index.tolist()
    map_code_table.append((len(map_code_table), 'None'))
    print(map_code_table)
    opp_map_code = map_code_table[-1][0]
    for i in range(len(map_code_table)):
        map_code_table[i] = list(map_code_table[i])
        if map_code_table[i][1] == opp_map:
            opp_map_code = map_code_table[i][0]
    print("Opponent Map Code is: " + str(opp_map_code) + "\n")

    # Find the records which match to opp_features
    race_found = (df_train['race_code'] == opp_race_code).any() 
    if race_found:
        print("Race Found.")
    opp_found = (df_train['name_code'] == opp_name_code).any() 
    if opp_found:
        print("Opp. Found.")
    map_found = (df_train['map_code'] == opp_map_code).any()
    if map_found:
        print("Map Found.")
    
    match_found = race_found and opp_found and map_found
    print("Records match attempted.")

    # No record, 1 record, and 2+ records
    if match_found:                        # 0 Record
        print("Multiple records matched")
        dfg_test_temp = df_train[given[:-1]]      # Set given features
        # Find max for binary feature and mean for numerical features
        det_max = dfg_test_temp['detector_count'].max()
        fly_max = dfg_test_temp['flyers'].max()
        dfg_test_cleaned = dfg_test_temp.mean()
        dfg_test = pd.DataFrame([(dfg_test_cleaned[0], dfg_test_cleaned[1], dfg_test_cleaned[2],
                                  dfg_test_cleaned[3], dfg_test_cleaned[4], dfg_test_cleaned[5],
                                  det_max, fly_max)], columns=given[:-1], index=[0])
        print(df_opening)
        print(dfg_test)
        df_final = generate_choose(df_opening,dfg_test)    # Set choosing features
        #Regardless: Pass results to C++ and tell us what we are working with.
        print(df_final)
        gas_proportion_t0 = df_final["gas_proportion"]
        print(gas_proportion_t0)
        supply_ratio_t0 = df_final["supply_ratio"]
        print(supply_ratio_t0)
        a_army_t0 = df_final["avg_army"]
        print(a_army_t0)
        a_econ_t0 = df_final["avg_econ"]
        print(a_econ_t0)
        a_tech_t0 = df_final["avg_tech"]
        print(a_tech_t0)
        r_out_t0 = df_final["r"]
        print(r_out_t0)
        build_order_t0 = df_final["opening_code"]
        print(build_order_t0)
        attempt_count = df_final["count"]
        print(attempt_count)
        print(abort_code_t0)
    else:
        print("Records match failed.")
        abort_code_t0 = True;
        print(abort_code_t0)

print("Script Concluded.")