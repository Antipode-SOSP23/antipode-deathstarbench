#!/usr/bin/env python3

import os
import glob
import re
from pprint import pprint
from pathlib import Path
import sys
import json
import ast
from datetime import datetime
from itertools import groupby
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
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

def _get(list, index, default):
  try:
    return list[index]
  except IndexError:
    return default

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

  pprint(data)
  # transform dict into dataframe
  df = pd.DataFrame.from_dict(data)

  # sort columns by the delay number in the string
  # sorted_columns = sorted(df.columns, key=lambda x: float(x.split('ms')[0]))
  # df = df.reindex(sorted_columns, axis=1)

  # plt.ylim([0, 100]) # percentage
  # plt.bar(np.arange(df.shape[1]), df.median(),
  #   yerr=[df.median()-df.min(), df.max()-df.median()],
  #   log=True,
  #   capsize=8,
  # )

  df.boxplot()
  plt.yscale('log')

  plt.ylabel('% of inconsistencies')
  plt.xlabel('DSB replication pair', labelpad=20)
  # add label at the top right of the plot
  ax = plt.gca()
  plt.text(1, 1, "$\it{150 qps}$   $\it{n=5}$", horizontalalignment='right', verticalalignment='bottom', transform=ax.transAxes)

  # replace xticks labels with dataframe info
  locs, labels = plt.xticks(np.arange(len(df.columns) + 1), [""] + df.columns.to_list(), horizontalalignment='right', rotation=45)

  # save with a unique timestamp
  plt.savefig(PLOTS_PATH / f"per_inconsistencies__{datetime.now().strftime('%Y%m%d%H%M')}", bbox_inches = 'tight', pad_inches = 0.1)


def plot__throughput_latency(args):
  data = {}
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)

    if 'antipode' in tag:
      baseline_or_antipode = 'antipode'
    else:
      baseline_or_antipode = 'baseline'

    latency_90 = None
    throughput = None
    with open(d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line latency
        if line.startswith(' 90.000%'):
          parts = re.split('(ms|s|m)', line.split('%')[1].strip())
          value = float(parts[0])
          unit = parts[1]

          # convert all units to ms
          if unit == 'm':
            latency_90 = (value * 60) * 1000
          elif unit == 's':
            latency_90 = (value * 1000)
          else: # ms
            latency_90 = value

          # round latency
          latency_90 = round(latency_90, 0)

        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    if tag not in data:
      data[tag] = []
    # insert at the position of the round
    data[tag].insert(tag_round - 1, { 'type': baseline_or_antipode, 'latency_90': latency_90, 'throughput': throughput})

  # since each tag/type has multiple rounds we have to group them
  # into a single row
  df_data = {}
  for tag, values in data.items():
    # group by baseline or antipode
    for t,vs in groupby(values, key=lambda x: x['type']):
      vs = list(vs)
      df_data[tag] = {
        'type': t,
        'latency_90': np.median([ v['latency_90'] for v in vs]),
        'throughput': np.median([ v['throughput'] for v in vs]),
      }

  # transform dict into dataframe
  df = pd.DataFrame.from_dict(df_data, orient='index')

  # index -> x -> throughput
  # values -> y -> latency_90
  # columns -> line
  fig, ax = plt.subplots()
  ax = sns.lineplot(data=df, x='throughput', y='latency_90', hue='type', markers=True, dashes=False)

  for gtype, item in df.groupby('type'):
    # move index labels to the dataframe for plotting as labels
    item.reset_index(inplace=True)
    # remove the antiopode tag from those rows
    item['index'] = item['index'].apply(lambda x: x.split(' - ')[0])
    for qps,throughput,latency in item[['index', 'throughput', 'latency_90']].values:
      ax.text(throughput,latency,qps)

  plt.ylabel('Latency (ms)\n$\it{Client\ side}$')
  plt.xlabel('Throughput (req/s)', labelpad=20)

  # save with a unique timestamp
  plt.savefig(PLOTS_PATH / f"throughput_latency__{datetime.now().strftime('%Y%m%d%H%M')}", bbox_inches = 'tight', pad_inches = 0.1)

def plot__throughput_visibility_latency(args):
  data = {}
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)

    if 'antipode' in tag:
      baseline_or_antipode = 'antipode'
    else:
      baseline_or_antipode = 'baseline'

    latency_90 = None
    throughput = None

    # get throughput from client file
    with open(d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    # get visibility latency from csv
    df = pd.read_csv(d / 'traces.csv', sep=';', index_col='ts')
    latency_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    if tag not in data:
      data[tag] = []
    # insert at the position of the round
    data[tag].insert(tag_round - 1, { 'type': baseline_or_antipode, 'latency_90': latency_90, 'throughput': throughput})

  # since each tag/type has multiple rounds we have to group them
  # into a single row
  df_data = {}
  for tag, values in data.items():
    # group by baseline or antipode
    for t,vs in groupby(values, key=lambda x: x['type']):
      vs = list(vs)
      df_data[tag] = {
        'type': t,
        'latency_90': np.median([ v['latency_90'] for v in vs]),
        'throughput': np.median([ v['throughput'] for v in vs]),
      }

  # transform dict into dataframe
  df = pd.DataFrame.from_dict(df_data, orient='index')

  # index -> x -> throughput
  # values -> y -> latency_90
  # columns -> line
  fig, ax = plt.subplots()
  ax = sns.lineplot(data=df, x='throughput', y='latency_90', hue='type', markers=True, dashes=False)

  for gtype, item in df.groupby('type'):
    # move index labels to the dataframe for plotting as labels
    item.reset_index(inplace=True)
    # remove the antiopode tag from those rows
    item['index'] = item['index'].apply(lambda x: x.split(' - ')[0])
    for qps,throughput,latency in item[['index', 'throughput', 'latency_90']].values:
      ax.text(throughput,latency,qps)

  plt.ylabel('Visibility Latency (ms)\n$\it{Difference\ from\ post\ to\ notification}$')
  plt.xlabel('Throughput (req/s)', labelpad=20)

  # save with a unique timestamp
  plt.savefig(PLOTS_PATH / f"throughput_visibility_latency__{datetime.now().strftime('%Y%m%d%H%M')}", bbox_inches = 'tight', pad_inches = 0.1)


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