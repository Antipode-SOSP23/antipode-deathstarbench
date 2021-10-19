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
        tag, tag_round = line.rstrip().split('GATHER TAG: ')[1].split(' --- round')
        # default tag_round to 1
        tag_round = tag_round.replace(' ', '') or 1
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
  sns.set_theme(style='ticks')

  data = []
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)
    tag = tag.split(' - ')[1].replace('->',r'$\rightarrow$')
    df = pd.read_csv(d / 'traces.csv', sep=';', index_col='ts')

    # compute extra info to output in info file
    consistent_per = round(len(df[df['consistency_bool'] == True])/float(len(df)) * 100, 2)
    inconsistent_per = round(len(df[df['consistency_bool'] == False])/float(len(df)) * 100, 2)

    # insert at the position of the round
    data.append({ tag : inconsistent_per})

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

  sns.boxplot(data=df, palette='tab10', width=0.2)
  # df.boxplot()
  # plt.yscale('log')

  plt.ylabel('% of inconsistencies')
  plt.xlabel('Regions', labelpad=20)
  # add label at the top right of the plot
  ax = plt.gca()
  # ax.set_ylim(0,100)
  # plt.text(1, 1, "$\it{150 rps}$   $\it{n=5}$", horizontalalignment='right', verticalalignment='bottom', transform=ax.transAxes)

  # replace xticks labels with dataframe info
  # plt.xticks(np.arange(len(df.columns) + 1), [""] + df.columns.to_list(), horizontalalignment='center', rotation=0)
  plt.xticks([0, 1], df.columns.to_list())

  # save with a unique timestamp
  plt.savefig(PLOTS_PATH / f"per_inconsistencies__{datetime.now().strftime('%Y%m%d%H%M')}", bbox_inches = 'tight', pad_inches = 0.1)


import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import Locator
class MinorSymLogLocator(Locator):
  """
  Dynamically find minor tick positions based on the positions of
  major ticks for a symlog scaling.
  """
  def __init__(self, linthresh):
      """
      Ticks will be placed between the major ticks.
      The placement is linear for x between -linthresh and linthresh,
      otherwise its logarithmically
      """
      self.linthresh = linthresh

  def __call__(self):
      'Return the locations of the ticks'
      majorlocs = self.axis.get_majorticklocs()

      # iterate through minor locs
      minorlocs = []

      # handle the lowest part
      for i in range(1, len(majorlocs)):
          majorstep = majorlocs[i] - majorlocs[i-1]
          if abs(majorlocs[i-1] + majorstep/2) < self.linthresh:
              ndivs = 10
          else:
              ndivs = 9
          minorstep = majorstep / ndivs
          locs = np.arange(majorlocs[i-1], majorlocs[i], minorstep)[1:]
          minorlocs.extend(locs)

      return self.raise_if_exceeds(np.array(minorlocs))

  def tick_values(self, vmin, vmax):
      raise NotImplementedError('Cannot get tick locations for a '
                                '%s type.' % type(self))

def plot__throughput_latency(args):
  parsed_data = []
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)
    info_tags = tag.split(' - ')

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

    # insert at the position of the round
    parsed_data.append({
      'rps': int(info_tags[0].split('qps')[0].split('rps')[0]),
      'zone_pair': info_tags[1],
      'type': _get(info_tags, 2, 'baseline'),
      'round': tag_round,
      'latency_90': latency_90,
      'throughput': throughput,
    })

  # since each tag/type has multiple rounds we have to group them into a single row
  df_data = []
  for t,vs in groupby(parsed_data, key=lambda x: [ x['rps'], x['zone_pair'], x['type'] ]):
    vs = list(vs)
    vs = sorted(vs, key=lambda k: (k['throughput'], k['latency_90']) )
    mid_index = int(len(vs) / 2)

    throughput = vs[mid_index]['throughput']
    latency_90 = vs[mid_index]['latency_90']
    # latency_90 = np.median([ v['latency_90'] for v in vs])
    # throughput = np.median([ v['throughput'] for v in vs])

    df_data.append({
      'rps': t[0],
      'zone_pair': t[1],
      'type': t[2],
      'latency_90': latency_90,
      'throughput': throughput,
    })

  # transform dict into dataframe
  df = pd.DataFrame(df_data)
  # split dataframe into multiple based on the amount of unique zone_pairs we have
  df_zone_pairs = [(x, pd.DataFrame(y)) for x, y in df.groupby('zone_pair', as_index=False)]

  # Apply the default theme
  sns.set_theme(style='ticks')
  plt.rcParams["figure.figsize"] = [6,5]

  # build the subplots
  fig, axes = plt.subplots(len(df_zone_pairs), 1)

  # Setting the values for all axes
  # xlims = (df['throughput'].min(), df['throughput'].max())
  # ylims = (df['latency_90'].min(), df['latency_90'].max())
  # plt.setp(axes, xlim=xlims, ylim=ylims)

  for i, (zone_pair, df_zone_pair) in enumerate(df_zone_pairs):
    # select the row of the subplot where to plot
    ax=axes[i]

    # drop uneeded columns
    df_zone_pair = df_zone_pair.drop(columns=['zone_pair'])
    # sort by rps so we get pretty walls - note the sort=False in lineplot
    df_zone_pair = df_zone_pair.sort_values(by=['rps'])

    # index -> x -> throughput -> effective throughput
    #            -> rps        -> client load
    # values -> y -> latency_90
    # columns -> line

    # from scipy import interpolate
    # interpolated_dfs = []
    # for deploy_type, df in [ (deploy_type, pd.DataFrame(y)) for deploy_type, y in df_zone_pair.groupby('type', as_index=False) ]:
    #   df = df.drop(columns='type')
    #   # array that will get interpolated values
    #   n = len(df)
    #   x_map = np.arange(0,n)
    #   dx = 0.4
    #   x_int = np.arange(0, n - 1, dx)  # vector where we interpolate
    #   df = pd.DataFrame({
    #     'type': [deploy_type] * len(x_int),
    #     'throughput': interpolate.interp1d(x_map, df['throughput'].values, 'quadratic')(x_int),
    #     'latency_90': interpolate.interp1d(x_map, df['latency_90'].values, 'quadratic')(x_int),
    #   })
    #   interpolated_dfs.append(df)
    # df_zone_pair = pd.concat(interpolated_dfs, ignore_index=False)

    ax.set(yscale="symlog")
    ax.yaxis.set_minor_locator(MinorSymLogLocator(1e-1))
    # ax.set_yticks([2000, 3000, 4000])
    sns.lineplot(ax=ax, data=df_zone_pair, sort=False, x='throughput', y='latency_90',
      hue='type', style='type', markers=False, dashes=False, linewidth = 3)

    # set limit for the y axis
    def format_tick(x, pos=None):
      return round(x*1000)

    # ax.yaxis.set_minor_formatter(matplotlib.ticker.ScalarFormatter())
    # ax.yaxis.set_minor_formatter(format_tick)

    # ax.set_ylim(0, 3000)

    # labels for data points
    # for gtype, item in df_zone_pair.groupby('type'):
    #   # move index labels to the dataframe for plotting as labels
    #   item.reset_index(inplace=True)

    #   for rps,throughput,latency in item[['rps', 'throughput', 'latency_90']].values:
    #     ax.text(throughput,latency, f"{int(rps)} rps", size='x-small')

    # set title for the zone pair
    ax.set_title(zone_pair.replace('->',r'$\rightarrow$'),)

    # clean ylabels so we set common ones after
    ax.set_ylabel('')
    ax.set_xlabel('')

    if i < len(df_zone_pairs) - 1:
      ax.set_xticklabels([])

    # get legend handles and labels
    handles, labels = ax.get_legend_handles_labels()
    # but remove the legend since we will only show one
    ax.get_legend().remove()


  # set a single legend for all the plots
  fig.legend(handles, labels, loc='upper right')

  # set a single yaxis title
  fig.supxlabel(y=0, t='Throughput (req/s)')
  fig.supylabel(x=-0.05, t='Latency (ms)\n$\it{Client\ side}$')

  # save with a unique timestamp
  # fig.tight_layout()
  plot_filename = f"throughput_latency__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(PLOTS_PATH / plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


def plot__throughput_visibility_latency(args):
  parsed_data = []
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)
    info_tags = tag.split(' - ')

    throughput = None
    with open(d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    # get visibility latency from csv
    df = pd.read_csv(d / 'traces.csv', sep=';', index_col='ts')
    latency_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    # insert at the position of the round
    parsed_data.append({
      'rps': int(info_tags[0].split('qps')[0].split('rps')[0]),
      'zone_pair': info_tags[1],
      'type': _get(info_tags, 2, 'baseline'),
      'round': tag_round,
      'latency_90': latency_90,
      'throughput': throughput,
    })

  # since each tag/type has multiple rounds we have to group them into a single row
  df_data = []
  for t,vs in groupby(parsed_data, key=lambda x: [ x['rps'], x['zone_pair'], x['type'] ]):
    vs = list(vs)
    df_data.append({
      'rps': t[0],
      'zone_pair': t[1],
      'type': t[2],
      'latency_90': np.median([ v['latency_90'] for v in vs]),
      'throughput': np.median([ v['throughput'] for v in vs]),
    })

  # transform dict into dataframe
  df = pd.DataFrame(df_data)
  # split dataframe into multiple based on the amount of unique zone_pairs we have
  df_zone_pairs = [(x, pd.DataFrame(y)) for x, y in df.groupby('zone_pair', as_index=False)]

  # build the subplots
  fig, axes = plt.subplots(len(df_zone_pairs), 1)

  # Setting the values for all axes
  # xlims = (df['throughput'].min(), df['throughput'].max())
  # ylims = (df['latency_90'].min(), df['latency_90'].max())
  # plt.setp(axes, xlim=xlims, ylim=ylims)

  for i, (zone_pair, df_zone_pair) in enumerate(df_zone_pairs):
    # select the row of the subplot where to plot
    ax=axes[i]

    # index -> x -> throughput
    # values -> y -> latency_90
    # columns -> line
    sns.lineplot(ax=ax, data=df_zone_pair, x='throughput', y='latency_90', hue='type', style='type', markers=True, dashes=False)

    for gtype, item in df_zone_pair.groupby('type'):
      # move index labels to the dataframe for plotting as labels
      item.reset_index(inplace=True)

      for rps,throughput,latency in item[['rps', 'throughput', 'latency_90']].values:
        ax.text(throughput,latency, f"{int(rps)} rps", size='x-small')

    # set title for the zone pair
    ax.set_title(zone_pair)

    # clean ylabels so we set common ones after
    ax.set_ylabel('')
    ax.set_xlabel('')

    if i < len(df_zone_pairs) - 1:
      ax.set_xticklabels([])

    # get legend handles and labels
    handles, labels = ax.get_legend_handles_labels()
    # but remove the legend since we will only show one
    ax.get_legend().remove()


  # set a single legend for all the plots
  fig.legend(handles, labels, bbox_to_anchor=(1.18, 0.5), loc='center right')

  # set a single yaxis title
  fig.supxlabel(y=0, t='Throughput (req/s)')
  fig.supylabel(x=0, t='Visibility Latency (ms)\n$\it{Client\ to\ notification}$')

  # save with a unique timestamp
  fig.tight_layout()
  plt.savefig(PLOTS_PATH / f"throughput_visibility_latency__{datetime.now().strftime('%Y%m%d%H%M')}", bbox_inches = 'tight', pad_inches = 0.1)


def plot__visibility_latency_overhead(args):
  parsed_data = []
  for d in args['dir']:
    tag, tag_round = _fetch_gather_tag(d)
    info_tags = tag.split(' - ')

    throughput = None
    with open(d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    # get visibility latency from csv
    df = pd.read_csv(d / 'traces.csv', sep=';', index_col='ts')
    latency_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    # insert at the position of the round
    parsed_data.append({
      'rps': int(info_tags[0].split('qps')[0].split('rps')[0]),
      'zone_pair': info_tags[1],
      'type': _get(info_tags, 2, 'baseline'),
      'round': tag_round,
      'latency_90': latency_90,
      'throughput': throughput,
    })

  # since each tag/type has multiple rounds we have to group them into a single row
  df_data = []
  for t,vs in groupby(parsed_data, key=lambda x: [ x['rps'], x['zone_pair'], x['type'] ]):
    vs = list(vs)
    df_data.append({
      'rps': t[0],
      'zone_pair': t[1],
      'type': t[2],
      'latency_90': np.median([ v['latency_90'] for v in vs]),
      'throughput': np.median([ v['throughput'] for v in vs]),
    })


  # transform dict into dataframe
  df = pd.DataFrame(df_data)
  # split dataframe into multiple based on the amount of unique zone_pairs we have
  df_zone_pairs = [(x, pd.DataFrame(y)) for x, y in df.groupby('zone_pair', as_index=False)]

  data = []
  for zone_pair,df_zone_pair in df_zone_pairs:
    data.append({
      'Regions': zone_pair.replace('->',r'$\rightarrow$'),
      'Baseline': round(df_zone_pair.loc[df_zone_pair.type=='baseline', 'latency_90'].values[0]),
      'Antipode': round(df_zone_pair.loc[df_zone_pair.type=='antipode', 'latency_90'].values[0]),
    })

  # for each Baseline / Antipode pair we take the Baseline out of antipode so
  # stacked bars are presented correctly
  for d in data:
    d['Antipode'] = max(0, d['Antipode'] - d['Baseline'])


  # Apply the default theme
  sns.set_theme(style='ticks')
  plt.figure(figsize=(4,3))

  df = pd.DataFrame.from_records(data).set_index('Regions')
  log = False
  if log:
    ax = df.plot(kind='bar', stacked=True, logy=True)
    ax.set_ylim(1, 100000)
    plt.xticks(rotation = 0)
  else:
    ax = df.plot(kind='bar', stacked=True, logy=False, width=0.2)
    plt.xticks(rotation = 0)

  ax.set_ylabel(r'Visibility Latency (ms)')

  # plot baseline bar
  ax.bar_label(ax.containers[0], label_type='center', fontsize=9, weight='bold', color='white')
  # plot overhead bar
  labels = [ f"+ {round(e)}" for e in ax.containers[1].datavalues ]
  ax.bar_label(ax.containers[1], labels=labels, label_type='edge', fontsize=9, weight='bold', color='black')


  # save with a unique timestamp
  plt.tight_layout()
  plot_filename = f"visibility_latency_overhead__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(PLOTS_PATH / plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")

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
  main_parser.add_argument('-d', '--dir', action='append', nargs='+', help="Use specific directory")
  main_parser.add_argument('-p', '--plot', required=True, action='append', choices=PLOT_NAMES, help="Plot the following plots")

  args = vars(main_parser.parse_args())

  if args['dir'] is None:
    args['dir'] = list()

  if args['latest']:
    all_ts_dirs = [ ts_dir for ts_dir in glob.glob(str(WKLD_DATA_PATH / '*' / '**')) if os.path.isdir(ts_dir) ]
    args['dir'].append(max(all_ts_dirs, key=os.path.getmtime))
  del args['latest']

  # flatten the list of dirs if needed - useful when aplying zsh /* + TAB combo
  args['dir'] = list(np.array(args['dir']).flat)
  # parse all paths as path and set all for their absolute paths
  # also filter paths that do not exist or empty folders
  args['dir'] = [ Path(d).absolute() for d in args['dir'] if os.path.exists(d) and os.path.isdir(d) and os.listdir(d) ]

  if not args['dir']:
    print("[WARN] No wkld data dirs available")
    exit(-1)

  for plot_name in args['plot']:
    print(f"[INFO] Plotting {plot_name} ...")
    getattr(sys.modules[__name__], f"plot__{plot_name}")(args)