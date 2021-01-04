#!/usr/bin/env python3

import os
from pprint import pprint
from pathlib import Path
import sys
import json
import ast
from datetime import datetime
import matplotlib.pyplot
import pandas as pd
pd.set_option('display.float_format', lambda x: '%.3f' % x)

#############################
# CONSTANTS
#
ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]

#############################
# HELPERS
#
def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']


# replace with argparse
exp_ts = '20201222131205'
# exp_ts = '20201217183942'
dataset_folder = ROOT_PATH / 'deploy' / 'wkld-data' / 'socialNetwork-gcp-colocated' / exp_ts

os.chdir(dataset_folder)

ats_df = pd.read_csv('ats_single.csv', sep=';')
ats_df = ats_df.set_index('ts')

vl_df = pd.read_csv('vl_single.csv', sep=';')
vl_df = vl_df.set_index('ts')

df = ats_df.merge(vl_df, left_on='ts', right_on='ts')
df = df.sort_values(by=['ts'])

ats_df=df.copy()
vl_df=df.copy()

# row of max queue duration
print(df.iloc[df['wht_queue_duration'].argmax()])

#
# del df['wht_antipode_duration']
# del df['wht_worker_duration']
del ats_df['wht_worker_per_antipode']
# del df['wht_queue_duration']
del ats_df['wht_total_duration']
#
del ats_df['post_notification_diff_ms']
del ats_df['replication_duration_ms']
del ats_df['mongodb_replication_duration_ms']
del ats_df['antipode_isvisible_duration_ms']

axs = ats_df.plot.line()

fig = axs.get_figure()
fig.savefig("plot_ats.png")

#
#
del vl_df['wht_antipode_duration']
del vl_df['wht_worker_duration']
del vl_df['wht_worker_per_antipode']
del vl_df['wht_queue_duration']
del vl_df['wht_total_duration']
#
del vl_df['post_notification_diff_ms']
# del vl_df['replication_duration_ms']
# del vl_df['mongodb_replication_duration_ms']
# del vl_df['antipode_isvisible_duration_ms']

axs = vl_df.plot.line()

fig = axs.get_figure()
fig.savefig("plot_vl.png")