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
exp_tss = [
  #
  # DISTRIBUTED
  #
  # -- 1 Client  // 75 req/s
  '20210216042308',
  # -- 1 Client  // 150 req/s
  # '20210126102219',
  #
  # CENTRALIZED
  #
  # -- 1 Client  // 75 req/s
  # '20210126131746',
  # -- 1 Client  // 150 req/s
  # '20210126123424'
]

for exp_ts in exp_tss:
  dataset_folder = ROOT_PATH / 'deploy' / 'wkld-data' / 'socialNetwork-gcp-colocated' / exp_ts

  os.chdir(dataset_folder)

  ats_df = pd.read_csv('ats_single.csv', sep=';')
  ats_df = ats_df.set_index('ts')

  vl_df = pd.read_csv('vl_single.csv', sep=';')
  vl_df = vl_df.set_index('ts')

  df = ats_df.merge(vl_df, left_on='ts', right_on='ts')
  df = df.sort_values(by=['ts'])

  # row of max queue duration
  print(df.iloc[df['wht_queue_duration'].argmax()])

  # from ats
  del df['wht_worker_per_antipode']
  del df['wht_total_duration']
  # from vl
  del df['post_notification_diff_ms']

  axs = df.plot.line(subplots=True, figsize=(5,10))

  axs[0].set_xticklabels([])

  fig = axs[0].get_figure()
  fig.savefig(f"plot_debug.png")




# distributed

# 20210126023656 -- 1 Client  // 75 req/s
# 20210126102219 -- 1 Client  // 150 req/s

# 20210126113934 -- 2 Clients // 75  req/s

# centralized

# 20210126131746 -- 1 Client  // 75 req/s
# 20210126123424 -- 1 Client  // 150 req/s
#  -- 2 Clients // 75  req/s
