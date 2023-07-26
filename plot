#!/usr/bin/env python3

import os
import re
from pprint import pprint as pp
from pathlib import Path
import sys
from datetime import datetime
from itertools import groupby
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import pandas as pd
pd.set_option('display.float_format', lambda x: '%.3f' % x)
from matplotlib.ticker import Locator
import yaml
import argparse

#-----------
# HELPERS
#-----------
def _load_yaml(path):
  import ruamel.yaml
  with open(path, 'r') as f:
    yaml = ruamel.yaml.YAML()
    yaml.preserve_quotes = True
    return yaml.load(f) or {}

def _dump_yaml(path, d):
  from ruamel.yaml import YAML
  yaml=YAML()
  path.parent.mkdir(exist_ok=True, parents=True)
  with open(path, 'w+') as f:
    yaml.default_flow_style = False
    yaml.dump(d, f)

def _fetch_span_tag(tags, tag_to_search):
  return next(item for item in tags if item['key'] == tag_to_search)['value']

def _convert_old_info(exp_dir):
  info = {}
  # find out if baseline or antipode
  info['type'] = 'antipode' if 'antipode' in exp_dir.parent.stem else 'baseline'
  # find zone pair and rps from gather tag
  with open(ROOT_PATH / exp_dir / 'traces.info') as f:
    lines = f.readlines()
    for line in lines:
      if line.startswith('GATHER TAG: '):
        parts = line.rstrip().split('GATHER TAG: ')[1].split(' --- round')[0].split(' - ')
        info['zone_pair'] = parts[1]
        # turn rps into number
        info['rps'] = int(parts[0].split('qps')[0].split('rps')[0].split('ms')[0])

  _dump_yaml(exp_dir / 'info.yml', info)
  print(f"[INFO] Generated new info file: {exp_dir / 'info.yml'}")

def _get(list, index, default):
  try:
    return list[index]
  except IndexError:
    return default

def _flatten_list(l):
  flatten_list = lambda irregular_list: [element for item in irregular_list for element in flatten_list(item)] if type(irregular_list) is list else [irregular_list]
  return flatten_list(l)

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


#-----------
# PLOTS
#-----------
def plot__per_inconsistencies(args):
  sns.set_theme(style='ticks')

  data = []
  for d in gather_paths:
    info = _load_yaml(d / 'info.yml')

    tag = info['zone_pair'].replace('->',r'$\rightarrow$')
    df = pd.read_csv(ROOT_PATH / d / 'traces.csv', sep=';', index_col='ts')

    # compute extra info to output in info file
    consistent_per = round(len(df[df['consistency_bool'] == True])/float(len(df)) * 100, 2)
    inconsistent_per = round(len(df[df['consistency_bool'] == False])/float(len(df)) * 100, 2)

    # insert at the position of the round
    data.append({ tag : inconsistent_per})

  # transform dict into dataframe
  df = pd.DataFrame.from_dict(data)
  pp(df)

  # sort columns by the delay number in the string
  # sorted_columns = sorted(df.columns, key=lambda x: float(x.split('ms')[0]))
  # df = df.reindex(sorted_columns, axis=1)

  # plt.ylim([0, 100]) # percentage
  # plt.bar(np.arange(df.shape[1]), df.median(),
  #   yerr=[df.median()-df.min(), df.max()-df.median()],
  #   log=True,
  #   capsize=8,
  # )

  ax = sns.boxplot(data=df, palette='tab10', width=0.2)
  # df.boxplot()
  # plt.yscale('log')

  plt.ylabel('% of inconsistencies')
  plt.xlabel('')
  # add label at the top right of the plot
  # ax = plt.gca()
  # ax.set_ylim(0,100)
  # plt.text(1, 1, "$\it{150 rps}$   $\it{n=5}$", horizontalalignment='right', verticalalignment='bottom', transform=ax.transAxes)

  # replace xticks labels with dataframe info
  # plt.xticks(np.arange(len(df.columns) + 1), [""] + df.columns.to_list(), horizontalalignment='center', rotation=0)
  plt.xticks([0, 1], df.columns.to_list())

  # save with a unique timestamp
  plot_filename = PLOTS_PATH / f"per_inconsistencies__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


def plot__throughput_latency(args):
  parsed_data = []
  for d in gather_paths:
    info = _load_yaml(d / 'info.yml')

    latency_90 = None
    throughput = None
    with open(ROOT_PATH / d / 'client01.out') as f:
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
      'rps': info['rps'],
      'zone_pair': info['zone_pair'],
      'type': info['type'],
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
      'type': 'Antipode' if t[2] == 'antipode' else 'Original',
      'latency_90': latency_90,
      'throughput': throughput,
    })

  # transform dict into dataframe
  df = pd.DataFrame(df_data)
  # split dataframe into multiple based on the amount of unique zone_pairs we have
  df_zone_pairs = [(x, pd.DataFrame(y)) for x, y in df.groupby('zone_pair', as_index=False)]

  # Apply the default theme
  sns.set_theme(style='ticks')
  plt.rcParams["figure.figsize"] = [6,4.5]
  plt.rcParams["figure.dpi"] = 600

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
    df_zone_pair = df_zone_pair.sort_values(by=['type', 'rps'])

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

    pp(df_zone_pair)

    ax.set(yscale="symlog")
    ax.yaxis.set_minor_locator(MinorSymLogLocator(1e-1))
    # ax.set_yticks([2000, 3000, 4000])

    color_palette = sns.color_palette("deep",2)[::-1]
    sns.lineplot(ax=ax, data=df_zone_pair, sort=False, x='throughput', y='latency_90',
      hue='type', style='type', palette=color_palette, markers=True, dashes=False, linewidth = 3)

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
    ax.set_title(zone_pair.replace('->',r'$\rightarrow$'),loc='right',fontdict={'fontsize': 'xx-small'}, style='italic')

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
  fig.legend(handles, labels, loc='upper left', bbox_to_anchor=(.125, 0.88))
  # sns.move_legend(fig, "lower center", bbox_to_anchor=(.5, 1), ncol=4, title=None, frameon=True)

  # set a single yaxis title
  fig.supxlabel(y=-0.01, t='Throughput (req/s)', fontsize='medium')
  fig.supylabel(t='Latency (ms)', fontsize='medium')

  # save with a unique timestamp
  # fig.tight_layout()
  plot_filename = f"throughput_latency__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(PLOTS_PATH / plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


def plot__throughput_visibility_latency(gather_paths):
  parsed_data = []
  for d in gather_paths:
    info = _load_yaml(d / 'info.yml')

    throughput = None
    with open(ROOT_PATH / d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    # get visibility latency from csv
    df = pd.read_csv(ROOT_PATH / d / 'traces.csv', sep=';', index_col='ts')
    latency_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    # insert at the position of the round
    parsed_data.append({
      'rps': info['rps'],
      'zone_pair': info['zone_pair'],
      'type': info['type'],
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
  fig.supylabel(x=0, t='Latency (ms)')

  # save with a unique timestamp
  fig.tight_layout()
  plot_filename = PLOTS_PATH / f"throughput_visibility_latency__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


def plot__visibility_latency_overhead(gather_paths):
  parsed_data = []
  for d in gather_paths:
    info = _load_yaml(d / 'info.yml')

    throughput = None
    with open(ROOT_PATH / d / 'client01.out') as f:
      lines = f.readlines()
      for line in lines:
        # this is the line for throughput
        if line.startswith('Requests/sec:'):
          throughput = float(line.split(':')[1].strip())

    # get visibility latency from csv
    df = pd.read_csv(ROOT_PATH / d / 'traces.csv', sep=';', index_col='ts')
    latency_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    # insert at the position of the round
    parsed_data.append({
      'rps': info['rps'],
      'zone_pair': info['zone_pair'],
      'type': info['type'],
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
      'Original': round(df_zone_pair.loc[df_zone_pair.type=='baseline', 'latency_90'].values[0]),
      'Antipode': round(df_zone_pair.loc[df_zone_pair.type=='antipode', 'latency_90'].values[0]),
    })

  # for each Baseline / Antipode pair we take the Baseline out of antipode so
  # stacked bars are presented correctly
  for d in data:
    d['Antipode'] = max(0, d['Antipode'] - d['Original'])


  # Apply the default theme
  sns.set_theme(style='ticks')
  plt.rcParams["figure.figsize"] = [6,2.9]
  plt.rcParams["figure.dpi"] = 600

  df = pd.DataFrame.from_records(data).set_index('Regions')
  log = False
  if log:
    ax = df.plot(kind='bar', stacked=True, logy=True)
    ax.set_ylim(1, 100000)
    plt.xticks(rotation = 0)
  else:
    ax = df.plot(kind='bar', stacked=True, logy=False, width=0.2)
    plt.xticks(rotation = 0)

  ax.set_ylabel(r'Latency (ms)')
  ax.set_xlabel('')

  # plot baseline bar
  ax.bar_label(ax.containers[0], label_type='center', fontsize=8, weight='bold', color='white')
  # plot overhead bar
  labels = [ f"+ {round(e)}" for e in ax.containers[1].datavalues ]
  ax.bar_label(ax.containers[1], labels=labels, label_type='edge', padding=-1, fontsize=8, weight='bold', color='black')

  # reverse order of legend
  handles, labels = ax.get_legend_handles_labels()
  ax.legend(handles[::-1], labels[::-1])

  # save with a unique timestamp
  plt.tight_layout()
  plot_filename = f"visibility_latency_overhead__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(PLOTS_PATH / plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


def plot__throughput_latency_with_consistency_window(args):
  parsed_data = []
  for d in gather_paths:
    info = _load_yaml(d / 'info.yml')

    latency_90 = None
    throughput = None
    with open(ROOT_PATH / d / 'client01.out') as f:
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

    # get visibility latency from csv
    df = pd.read_csv(ROOT_PATH / d / 'traces.csv', sep=';', index_col='ts')
    consistency_window_90 = np.percentile(df[['post_notification_diff_ms']], 90)

    # insert at the position of the round
    parsed_data.append({
      'rps': info['rps'],
      'zone_pair': info['zone_pair'],
      'type': info['type'],
      'latency_90': latency_90,
      'consistency_window_90': consistency_window_90,
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
      'type': 'Antipode' if t[2] == 'antipode' else 'Original',
      'latency_90': latency_90,
      'throughput': throughput,
      # 'consistency_window_90': np.median([ v['consistency_window_90'] for v in vs]),
      'consistency_window_90': [ v['consistency_window_90'] for v in vs],
    })

  # transform dict into dataframe
  df = pd.DataFrame(df_data)

  # split dataframe into multiple based on the amount of unique zone_pairs we have
  PEAK_RPS = 125
  df_zone_pairs = []
  for zone_pair, df_zone_pair in df.groupby('zone_pair', as_index=False):
    # drop uneeded columns
    df_zone_pair = df_zone_pair.drop(columns=['zone_pair'])
    # sort by rps so we get pretty walls - note the sort=False in lineplot
    df_zone_pair = df_zone_pair.sort_values(by=['type', 'rps'])

    # gather all consistency window samples from Original and Antipode and then compute the median
    cw_data = {
      'Throughput': PEAK_RPS,
      'Original': round(np.median(_flatten_list(df_zone_pair[(df_zone_pair['type'] == 'Original') & (df_zone_pair['rps'] == PEAK_RPS)]['consistency_window_90'].values.tolist()))),
      'Antipode': round(np.median(_flatten_list(df_zone_pair[(df_zone_pair['type'] == 'Antipode') & (df_zone_pair['rps'] == PEAK_RPS)]['consistency_window_90'].values.tolist()))),
    }
    # for each Baseline / Antipode pair we take the Baseline out of antipode so
    # stacked bars are presented correctly
    cw_data['Antipode'] = max(0, cw_data['Antipode'] - cw_data['Original'])
    cw_df = pd.DataFrame.from_records([cw_data]).set_index('Throughput')

    # index -> x -> throughput -> effective throughput
    #            -> rps        -> client load
    # values -> y -> latency_90
    #           y -> consistency_window_90
    # columns -> line
    df_zone_pairs.append((zone_pair, df_zone_pair, cw_df))

  # Apply the default theme
  sns.set_theme(style='ticks')
  plt.rcParams["figure.figsize"] = [6,4.25]
  plt.rcParams["figure.dpi"] = 600
  plt.rcParams['axes.labelsize'] = 'small'

  # build the subplots
  fig, axes = plt.subplots(len(df_zone_pairs), 2, gridspec_kw={'wspace':0.05, 'hspace':0.23, 'width_ratios': [4, 1]})

  # values to apply to all plots
  cw_ylim = max([ cw_df.sum(axis=1).tolist()[0] for (_,_,cw_df) in df_zone_pairs ]) * 1.075
  tl_xlim_right = df['throughput'].max() * 1.03
  # tl_xlim_left = df['throughput'].min() * 0.8

  for i, (zone_pair, df_zone_pair, cw_df) in enumerate(df_zone_pairs):
    #---------------
    # Throughput / Latency part
    #---------------
    # select the row of the subplot where to plot
    tl_ax=axes[i][0]

    tl_ax.set(yscale="symlog")
    tl_ax.yaxis.set_minor_locator(MinorSymLogLocator(1e-1))

    color_palette = sns.color_palette("deep",2)[::-1]
    sns.lineplot(ax=tl_ax, data=df_zone_pair, sort=False, x='throughput', y='latency_90',
      hue='type', style='type', palette=color_palette, markers=True, dashes=False, linewidth = 3)

    # set title for the zone pair
    tl_ax.set_title(zone_pair.replace('->',r'$\rightarrow$'),
      loc='left',fontdict={'fontsize': 'small'}, style='italic')

    # common xlim for both plots
    tl_ax.set_xlim(right=tl_xlim_right)

    if i == 0:
      # only keep labels on bottom plot
      tl_ax.set_xlabel('')
      tl_ax.set_ylabel('')
      tl_ax.set_xticklabels([])
      # remove title from legends
      tl_ax.legend_.set_title(None)
    elif i == 1:
      tl_ax.set_xlabel('Throughput (req/s)')
      tl_ax.set_ylabel('Latency (ms)', y=1.1)
      # only keep legend on top plot
      tl_ax.get_legend().remove()

    #---------------
    # Consistency window
    #---------------
    # select the row of the subplot where to plot
    cw_ax=axes[i][1]

    cw_df.plot(ax=cw_ax, kind='bar', stacked=True, logy=False, width=0.4)

    # set axis labels
    cw_ax.set_ylabel(r'Consistency Window (ms)')
    cw_ax.set_xlabel('')

    # plot baseline bar
    cw_ax.bar_label(cw_ax.containers[0], label_type='center', fontsize=8, weight='bold', color='white')
    # plot overhead bar
    cw_ax.bar_label(cw_ax.containers[1], labels=[ f"+ {round(e)}" for e in cw_ax.containers[1].datavalues ],
      label_type='edge', padding=-1, fontsize=8, weight='bold', color='black')

    # remove legend
    cw_ax.get_legend().remove()

    # place yticks on the right
    cw_ax.yaxis.tick_right()

    # same limits to both plots
    cw_ax.set_ylim(bottom=0, top=cw_ylim)

    # only show one yaxis label
    if i == 0:
      # mention that consistency window is for runs with 125rps only ??
      cw_ax.set_title(f"peak req/s",loc='right',fontdict={'fontsize': 'xx-small'}, style='italic')
      cw_ax.set_ylabel('')
      cw_ax.set_xticklabels([])
    elif i == 1:
      cw_ax.set_title('')
      cw_ax.set_ylabel('Consistency Window (ms)', y=1.1)
      cw_ax.yaxis.set_label_position('right')
      cw_ax.tick_params(axis='x', labelrotation=0)

    # set title for the zone pair

  # save with a unique timestamp
  # fig.tight_layout()
  plot_filename = f"throughput_latency_with_consistency_window__{datetime.now().strftime('%Y%m%d%H%M')}"
  plt.savefig(PLOTS_PATH / plot_filename, bbox_inches = 'tight', pad_inches = 0.1)
  print(f"[INFO] Saved plot '{plot_filename}'")


#-----------
# CONSTANTS
#-----------
ROOT_PATH = Path(os.path.abspath(os.path.dirname(sys.argv[0])))
PLOTS_PATH = ROOT_PATH / 'plots'
PERCENTILES_TO_PRINT = [.25, .5, .75, .90, .99]
# plot names have to be AFTER the method definitions
PLOT_NAMES = [ m.split('plot__')[1] for m in dir(sys.modules[__name__]) if m.startswith('plot__') ]


#--------------
# CLI
#--------------
if __name__ == '__main__':
  # parse arguments
  main_parser = argparse.ArgumentParser()
  main_parser.add_argument('config', type=argparse.FileType('r', encoding='UTF-8'), help="Plot config to load")
  main_parser.add_argument('--plots', nargs='*', choices=PLOT_NAMES, default=PLOT_NAMES, required=False, help="Plot only the passed plot names")

  # parse args
  args = vars(main_parser.parse_args())

  # load yaml
  args['config'] = (yaml.safe_load(args['config']) or {})

  for plot_name in set(args['config'].keys()) & set(args['plots']):
    gather_paths = [ Path(p) for p in args['config'][plot_name] ]

    # Temporary fix for migrating from tags to info.yml file
    for p in gather_paths:
      if not Path(p / 'info.yml').is_file():
        _convert_old_info(p)

    getattr(sys.modules[__name__], f"plot__{plot_name}")(gather_paths)