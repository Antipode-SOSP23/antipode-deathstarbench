#!/usr/bin/env python3

import os
import glob
from pprint import pprint
from pathlib import Path
import sys
import json
import ast
from datetime import datetime
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
pd.set_option('display.float_format', lambda x: '%.3f' % x)

#-----------
# HELPERS
#-----------
def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']

def _fetch_gather_tag(exp_dir):
  # get the tag of this experiment
  with open(exp_dir / 'traces.info') as f:
    lines = f.readlines()
    for line in lines:
      if line.startswith('GATHER TAG: '):
        tag, tag_round = line.rstrip().split('GATHER TAG: ')[1].split(' --- round ')
        return tag, int(tag_round)

#-----------
# PLOTS
#-----------
def plot__debug(dataset_folder):
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


def plot__per_inconsistencies(args):
  data = {}
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)
    df = pd.read_csv(d / 'traces.csv', sep=';', index_col='ts')

    # compute extra info to output in info file
    consistent_per = round(len(df[df['consistency_bool'] == True])/float(len(df)) * 100, 2)
    inconsistent_per = round(len(df[df['consistency_bool'] == False])/float(len(df)) * 100, 2)

    if tag not in data:
      data[tag] = []
    # insert at the position of the round
    data[tag].insert(tag_round - 1, inconsistent_per)

  # transform dict into dataframe
  df = pd.DataFrame.from_dict(data)
  pprint(df)

  # plt.ylim([0, 100]) # percentage
  plt.bar(np.arange(df.shape[1]), df.median(),
    yerr=[df.median()-df.min(), df.max()-df.median()],
    log=True,
    capsize=8,
  )
  plt.ylabel('Inconsistency %')
  plt.xlabel('DSB replication pair')

  # replace xticks labels with dataframe info
  locs, labels = plt.xticks(np.arange(len(df.columns)), df.columns)

  # save with a unique timestamp
  plt.savefig(PLOTS_PATH / f"per_inconsistencies__{datetime.now().strftime('%Y%m%d%H%M')}")

#-----------
# CONSTANTS
#-----------
ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))
WKLD_DATA_PATH = ROOT_PATH / 'deploy' / 'wkld-data'
PLOTS_PATH = ROOT_PATH / 'deploy' / 'plots'
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]
# plot names have to be AFTER the method definitions
PLOT_NAMES = [ m.split('plot__')[1] for m in dir(sys.modules[__name__]) if m.startswith('plot__') ]

#-----------
# MAIN
#-----------
if __name__ == "__main__":
  import argparse

  # parse arguments
  main_parser = argparse.ArgumentParser()

  main_parser.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  main_parser.add_argument('-d', '--dir', action='append', help="Use specific directory")
  main_parser.add_argument('-p', '--plot', required=True, action='append', choices=PLOT_NAMES, help="Plot the following plots")

  args = vars(main_parser.parse_args())

  if args['dir'] is None:
    args['dir'] = list()

  if args['latest']:
    all_ts_dirs = [ ts_dir for ts_dir in glob.glob(str(WKLD_DATA_PATH / '*' / '**')) if os.path.isdir(ts_dir) ]
    args['dir'].append(max(all_ts_dirs, key=os.path.getmtime))
  del args['latest']

  # parse all paths as path and set all for their absolute paths
  # also filter paths that do not exist or empty folders
  args['dir'] = [ Path(d).absolute() for d in args['dir'] if os.path.exists(d) and os.path.isdir(d) and os.listdir(d) ]

  if not args['dir']:
    print("[WARN] No wkld data dirs available")
    exit(-1)

  for plot_name in args['plot']:
    print(f"[INFO] Plotting {plot_name} ...")
    getattr(sys.modules[__name__], f"plot__{plot_name}")(args)