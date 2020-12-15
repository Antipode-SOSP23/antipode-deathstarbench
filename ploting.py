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
dataset_folder = ROOT_PATH / 'deploy' / 'wkld-data' / 'socialNetwork-gcp-colocated' / '20201215113248'

os.chdir(dataset_folder)

with open('jaeger.json', 'r') as json_file:
  traces = []

  data = json_file.read()
  content = ast.literal_eval(data)

  for trace in content['data']:
    trace_info = {
      'ts': None,
      # 'trace_id': trace['spans'][0]['traceID'],
      'post_id': None,
      # 'antipode_isvisible_duration': -1,
      # 'antipode_isvisible_attempts': -1,
      'wht_start_queue_ts': None,
      'wth_start_worker_ts': None,
      'wth_end_worker_ts': None,
      'wht_antipode_duration': None,
    }

    # search trace info in different spans
    for s in trace['spans']:
      if s['operationName'] == '_ComposeAndUpload':
        trace_info['post_id'] = float(_fetch_span_tag(s['tags'], 'composepost_id'))
        trace_info['ts'] = datetime.fromtimestamp(s['startTime']/1000000.0)

      if s['operationName'] == '_UploadHomeTimelineHelper':
        # compute the time spent in the queue
        trace_info['wht_start_queue_ts'] = float(_fetch_span_tag(s['tags'], 'wht_start_queue_ts'))

      # these values are captured by wht_antipode_duration
      # if s['operationName'] == 'IsVisible':
        # trace_info['antipode_isvisible_duration'] = float(_fetch_span_tag(s['tags'], 'antipode_isvisible_duration'))
        # trace_info['antipode_isvisible_attempts'] = int(_fetch_span_tag(s['tags'], 'antipode_isvisible_attempts'))

      if s['operationName'] == 'FanoutHomeTimelines':
        # duration spent in antipode
        trace_info['wht_antipode_duration'] = float(_fetch_span_tag(s['tags'], 'wht_antipode_duration'))
        # compute the time spent in the queue
        trace_info['wth_start_worker_ts'] = float(_fetch_span_tag(s['tags'], 'wth_start_worker_ts'))
        # total time spent in antipode operations while in WHT
        trace_info['wth_end_worker_ts'] = float(_fetch_span_tag(s['tags'], 'wth_end_worker_ts'))

    # skip if we still have -1 values
    if any(v is None for v in trace_info.values()):
      # print(f"[INFO] trace missing information: {trace_info}")
      continue

    # total time of worker
    diff = datetime.fromtimestamp(trace_info['wth_end_worker_ts']/1000.0) - datetime.fromtimestamp(trace_info['wth_start_worker_ts']/1000.0)
    trace_info['wht_worker_duration'] = float(diff.total_seconds() * 1000)

    try:
      trace_info['wht_worker_per_antipode'] = trace_info['wht_antipode_duration'] / trace_info['wht_worker_duration']
    except ZeroDivisionError:
      trace_info['wht_worker_per_antipode'] = 1

    # computes time spent queued in rabbitmq
    diff = datetime.fromtimestamp(trace_info['wth_start_worker_ts']/1000.0) - datetime.fromtimestamp(trace_info['wht_start_queue_ts']/1000.0)
    trace_info['wht_queue_duration'] = float(diff.total_seconds() * 1000)

    # queue + worker time
    diff = datetime.fromtimestamp(trace_info['wth_end_worker_ts']/1000.0) - datetime.fromtimestamp(trace_info['wht_start_queue_ts']/1000.0)
    trace_info['wht_total_duration'] = float(diff.total_seconds() * 1000)

    traces.append(trace_info)

  df = pd.DataFrame(traces)
  del df['post_id']
  del df['wht_start_queue_ts']
  del df['wth_start_worker_ts']
  del df['wth_end_worker_ts']
  del df['wht_worker_per_antipode']
  #
  del df['wht_total_duration']

  df = df.set_index('ts')
  axs = df.plot.line()

  fig = axs.get_figure()
  fig.savefig("plot.png")

  # --
  print(df.describe(percentiles=PERCENTILES_TO_PRINT))
