require "socket"
math.randomseed(socket.gettime()*1000)
math.random(); math.random(); math.random()
JSON = require("JSON")
stringx = require "pl.stringx"
tablex = require "pl.tablex"

local charset = {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', 'z', 'x', 'c', 'v', 'b', 'n', 'm', 'Q',
  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H',
  'J', 'K', 'L', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '1', '2', '3', '4', '5',
  '6', '7', '8', '9', '0'}

local decset = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}

local function stringRandom(length)
  if length > 0 then
    return stringRandom(length - 1) .. charset[math.random(1, #charset)]
  else
    return ""
  end
end

local function decRandom(length)
  if length > 0 then
    return decRandom(length - 1) .. decset[math.random(1, #decset)]
  else
    return ""
  end
end

local function findPostId(entries, post_id)
  for _,entry in ipairs(entries) do
    if entry['post_id'] == post_id then
      return true
    end
  end
  return false
end

local post_id = nil

request = function()
  -- local user_index = math.random(2, 961)
  local user_index = 960
  local username = "username_" .. tostring(user_index)
  local user_id = tostring(user_index)
  local text = stringRandom(256)
  local num_user_mentions = -1 -- math.random(0, 5)
  local num_urls = -1 -- math.random(0, 5)
  local num_media = -1 --math.random(0, 4)
  local media_ids = '['
  local media_types = '['

  -- find follower
  local dataset_path = debug.getinfo(1,'S').source:match("(.*/)")
  dataset_path = string.sub(dataset_path, 2)
  dataset_path = dataset_path .. "../../../datasets/social-graph/socfb-Reed98/socfb-Reed98.mtx"

  local followers = {}
  for line in io.lines(dataset_path) do
    local id1, id2 = string.match(line, "(%d+)%s+(%d+)")

    if id1 == user_id then
      table.insert(followers, id2)
    end
    if id2 == user_id then
      table.insert(followers, id1)
    end
  end
  follower_id = followers[math.random(#followers)]

  -- compose post
  for i = 0, num_user_mentions, 1 do
    local user_mention_id
    while (true) do
      user_mention_id = math.random(1, 962)
      if user_index ~= user_mention_id then
        break
      end
    end
    text = text .. " @username_" .. tostring(user_mention_id)
  end

  for i = 0, num_urls, 1 do
    text = text .. " http://" .. stringRandom(64)
  end

  for i = 0, num_media, 1 do
    local media_id = decRandom(18)
    media_ids = media_ids .. "\"" .. media_id .. "\","
    media_types = media_types .. "\"png\","
  end

  media_ids = media_ids:sub(1, #media_ids - 1) .. "]"
  media_types = media_types:sub(1, #media_types - 1) .. "]"

  local method = "POST"
  local path = os.getenv('HOST_EU') .. "/wrk2-api/post/compose"
  local headers = {}
  local body
  headers["Content-Type"] = "application/x-www-form-urlencoded"
  body = "username=" .. username .. "&user_id=" .. user_id ..
         "&text=" .. text .. "&post_type=0"

  local http = require("socket.http")
  local body, code, headers, status = http.request(path, body)

  if code == 200 then
    post_id = stringx.strip(stringx.split(body, '#')[2])

    -- now reads the timeline
    -- local start = tostring(math.random(0, 100))
    -- local stop = tostring(start + 100)
    local start = 0;
    local stop = 10;

    local args = "user_id=" .. follower_id .. "&start=" .. start .. "&stop=" .. stop
    local method = "GET"
    local headers = {}
    headers["Content-Type"] = "application/x-www-form-urlencoded"
    local path = os.getenv('HOST_US') .. "/wrk2-api/home-timeline/read?" .. args

    return wrk.format(method, path, headers, nil)
  end
end

-- response = function(status, headers, body)
--   print(status)
--   if status == 200 then
--     decode = JSON:decode(body)
--     print(tablex.size(decode))

--     if findPostId(decode, post_id) then
--       print("FOUND")
--     else
--       print("NOT_FOUND")
--     end
--   end
-- end