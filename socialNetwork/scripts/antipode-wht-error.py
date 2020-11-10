import random
from faker import Faker
fake = Faker()
import requests
from pprint import pprint
import time
import json
import os

if not('HOST_EU' in os.environ and 'HOST_US' in os.environ):
  print("[ERROR] Set HOST_EU and HOST_US env vars with host uri for the nginx endpoints")
  exit(-1)

def _register_user(user_id):
  data = {
    'user_id' : user_id,
    'username' : f"username_{user_id}",
    'first_name': f"first_name_{user_id}",
    'last_name': f"last_name_{user_id}",
    'password': f"password_{user_id}",
  }
  # make first request to compose the post
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  r = requests.post(f"{os.environ['HOST_EU']}/wrk2-api/user/register", data=data, headers=headers)
  print(f"[Register User] {r.status_code}, {r.reason}")
  if r.status_code == 200:
    pprint(data)
  else:
    print(f"[Register User] ERROR {r.status_code}, {r.reason}")


def _follow_user(followee_id, follower_id):
  data = {
    'user_name': f"username_{follower_id}",
    'followee_name' : f"username_{followee_id}",
  }
  # make first request to compose the post
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  r = requests.post(f"{os.environ['HOST_EU']}/wrk2-api/user/follow", data=data, headers=headers)
  print(f"[Follow User] {r.status_code}, {r.reason}")
  if r.status_code == 200:
    pprint(data)
  else:
    print(f"[Follow User] ERROR {r.status_code}, {r.reason}")
    exit()


def _create_post(user_id):
  # data for url request
  data = {
    'user_id' : user_id,
    'username' : f"username_{user_id}",
    'text' : fake.text(),
    'media_ids' : [],
    'media_types' : [],
    'post_type' : 0,
  }
  # make first request to compose the post
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  r = requests.post(f"{os.environ['HOST_EU']}/wrk2-api/post/compose", data=data, headers=headers)
  print(f"[Compose Post] {r.status_code}, {r.reason}")
  if r.status_code == 200:
    pprint(data)
  else:
    print(f"[Compose Post] ERROR {r.status_code}, {r.reason}")
    exit()


def _read_home_timeline(user_id):
  # make second request to read the user timeline
  wht_params = {
    'user_id' : user_id,
    'start' : 0,
    'stop' : 10000,
  }
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  r = requests.get(f"{os.environ['HOST_US']}/wrk2-api/home-timeline/read", params=wht_params)
  print(f"[Read Home Timeline] {r.status_code}, {r.reason}")
  if r.status_code == 200:
    json_content = json.loads(r.content)
    print(f"[Read Home Timeline] Loaded {len(json_content)} records")
    # obj = json_content[-1]
    pprint(json_content)
  else:
    print(f"[Read Home Timeline] ERROR {r.status_code}, {r.reason}")


def _read_user_timeline(user_id):
  # make second request to read the user timeline
  wht_params = {
    'user_id' : user_id,
    'start' : 0,
    'stop' : 100,
  }
  headers = {'Content-Type': 'application/x-www-form-urlencoded'}
  r = requests.get(f"{os.environ['HOST_US']}/wrk2-api/user-timeline/read", params=wht_params)
  print(f"[Read User Timeline] {r.status_code}, {r.reason}")
  if r.status_code == 200:
    json_content = json.loads(r.content)
    # obj = json_content[-1]
    pprint(json_content)
  else:
    print(f"[Read User Timeline] ERROR {r.status_code}, {r.reason}")


#---------
# Create main user
#
# user_id = random.randrange(1, 962)
user_id = 962
_register_user(user_id)


#---------
# Create followers
#
# follower_ids = []
# for i in range(3):
#   fid = None
#   while(True):
#     # fid = random.randrange(1, 962)
#     # fid = user_id + i + 1
#     fid = 624

#     if user_id != fid:
#       break
#   # _register_user(fid)
#   _follow_user(fid, user_id)
#   follower_ids.append(fid)

follower_id = 624
_register_user(follower_id)
_follow_user(user_id, follower_id)

#---------
# Create post
#
_create_post(user_id)

time.sleep(10)

#---------
# Read user timeline
#
# input("Waiting for the post to be available...")
_read_home_timeline(follower_id)
# _read_user_timeline(user_id)
