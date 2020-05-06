import random
from faker import Faker
fake = Faker()
import requests
from pprint import pprint
import time
import json

# data common to both requests
user_id = random.randrange(1, 962)

# data for url request
compose_data = {
  'user_id' : user_id,
  'username' : f"username_{user_id}",
  'text' : fake.text(),
  'media_ids' : [],
  'media_types' : [],
  'post_type' : 0,
}
# make first request to compose the post
headers = {'Content-Type': 'application/x-www-form-urlencoded'}
r = requests.post('http://localhost:8080/wrk2-api/post/compose', data=compose_data, headers=headers)
print(f"[Compose Post] {r.status_code}, {r.reason}")
if r.status_code == 200:
  pprint(compose_data)
else:
  print(f"[Compose Post] ERROR {r.status_code}, {r.reason}")
  exit()


input("Waiting for the post to be available...")

# make second request to read the user timeline
wht_params = {
  'user_id' : user_id,
  'start' : 0,
  'stop' : 100,
}
headers = {'Content-Type': 'application/x-www-form-urlencoded'}
r = requests.get('http://localhost:8080/wrk2-api/user-timeline/read', params=wht_params)
print(f"[Read User Timeline] {r.status_code}, {r.reason}")
if r.status_code == 200:
  obj = json.loads(r.content)[-1]
  pprint(obj)
else:
  print(f"[Read User Timeline] ERROR {r.status_code}, {r.reason}")
