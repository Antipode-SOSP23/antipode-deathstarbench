#!/usr/bin/env python3

import os
import glob
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
WKLD_DATA_PATH = ROOT_PATH / 'deploy' / 'wkld-data'
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]

#############################
# HELPERS
#
def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']

#############################
# HELPERS
#
def plot_debug(dataset_folder):
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


if __name__ == "__main__":
  import argparse

  # parse arguments
  main_parser = argparse.ArgumentParser()

  deploy_file_group = main_parser.add_mutually_exclusive_group(required=True)
  deploy_file_group.add_argument('-l', '--latest', action='store_true', help="Use last used deploy file")
  deploy_file_group.add_argument('-d', '--dir', help="Use specific directory")

  args = vars(main_parser.parse_args())

  if args['latest'] == True:
    all_ts_dirs = [ ts_dir for ts_dir in glob.glob(str(WKLD_DATA_PATH / '*' / '**')) if os.path.isdir(ts_dir) ]
    args['dir'] = max(all_ts_dirs, key=os.path.getmtime)

  # check if directory is not empty
  if os.path.exists(args['dir']) and os.path.isdir(args['dir']):
    if not os.listdir(args['dir']):
      raise argparse.ArgumentError(args['dir'], "is an empty dir.")
  else:
    raise argparse.ArgumentError(args['dir'], "does not exist")

  # no errors call plot - for now we only have one
  plot_debug(args['dir'])