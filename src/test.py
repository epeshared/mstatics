#!/usr/bin/python3


import random
import pandas as pd

# Some sample data to plot.
cat_4 = ['Metric_' + str(x) for x in range(1, 9)]
index_4 = ['Data 1', 'Data 2', 'Data 3', 'Data 4']
data_3 = {}
for cat in cat_4:
    data_3[cat] = [random.randint(10, 100) for x in index_4]
df = pd.DataFrame(data_3, index=index_4)
print(df)