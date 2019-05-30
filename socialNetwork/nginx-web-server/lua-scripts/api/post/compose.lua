local _M = {}
local xtracer = require "luaxtrace"

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

local function _UploadUserId(req_id, user_id, username, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local UserServiceClient = require "social_network_UserService"
  local ngx = ngx

  local user_client = GenericObjectPool:connection(
      UserServiceClient, "user-service", 9090)
  carrier["baggage"] = xtracer.BranchBaggage()
  local status, err = pcall(user_client.UploadCreatorWithUserId, user_client,
      req_id, user_id, username, carrier)
  if not status then
    ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
    ngx.say("Follow Failed: " .. err.message)
    ngx.log(ngx.ERR, "Follow Failed: " .. err.message)
    xtracer.LogXTrace("Follow Failed: " .. err.message)
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
  end
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(user_client)
  xtracer.DeleteBaggage()
end

local function _UploadText(req_id, post, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local TextServiceClient = require "social_network_TextService"
  local text_client = GenericObjectPool:connection(
      TextServiceClient, "text-service", 9090)
  carrier["baggage"] = xtracer.BranchBaggage()
  local status, err = pcall(text_client.UploadText, text_client, req_id,
      post.text, carrier)
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(text_client)
  xtracer.DeleteBaggage()
end

local function _UploadUniqueId(req_id, post, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local UniqueIdServiceClient = require "social_network_UniqueIdService"
  local unique_id_client = GenericObjectPool:connection(
      UniqueIdServiceClient, "unique-id-service", 9090)
  carrier["baggage"] = xtracer.BranchBaggage()
  local status, err = pcall(unique_id_client.UploadUniqueId, unique_id_client,
      req_id, tonumber(post.post_type), carrier)
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(unique_id_client)
  xtracer.DeleteBaggage()
end

local function _UploadMedia(req_id, post, carrier)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local MediaServiceClient = require "social_network_MediaService"
  local cjson = require "cjson"

  carrier["baggage"] = xtracer.BranchBaggage()
  local media_client = GenericObjectPool:connection(
      MediaServiceClient, "media-service", 9090)
  if (not _StrIsEmpty(post.media_ids) and not _StrIsEmpty(post.media_types)) then
    local status, err = pcall(media_client.UploadMedia, media_client,
        req_id, cjson.decode(post.media_types), cjson.decode(post.media_ids), carrier)
  else
    local status, err = pcall(media_client.UploadMedia, media_client,
        req_id, {}, {}, carrier)
  end
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(media_client)
  xtracer.DeleteBaggage()
end

function _M.ComposePost()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local ngx = ngx
  local cjson = require "cjson"
  local jwt = require "resty.jwt"

  xtracer.StartLuaTrace("NginxWebServer", "ComposePost")
  xtracer.LogXTrace("Processing request")
  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("ComposePost",
      { ["references"] = { { "child_of", parent_span_context } } })
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  if (_StrIsEmpty(post.post_type) or _StrIsEmpty(post.text)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtracer.LogXTrace("Bad Request - Incomplete Arguments")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  if (_StrIsEmpty(ngx.var.cookie_login_token)) then
    ngx.status = ngx.HTTP_UNAUTHORIZED
    xtracer.LogXTrace("Bad Request - Unauthorized")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_OK)
  end

  local login_obj = jwt:verify(ngx.shared.config:get("secret"), ngx.var.cookie_login_token)
  if not login_obj["verified"] then
    ngx.status = ngx.HTTP_UNAUTHORIZED
    ngx.say(login_obj.reason);
    xtracer.LogXTrace("Bad Request - Unauthorized")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_OK)
  end
  local timestamp = tonumber(login_obj["payload"]["timestamp"])
  local ttl = tonumber(login_obj["payload"]["ttl"])
  local user_id = tonumber(login_obj["payload"]["user_id"])
  local username = login_obj["payload"]["username"]

  if (timestamp + ttl < ngx.time()) then
    ngx.status = ngx.HTTP_UNAUTHORIZED
    ngx.header.content_type = "text/plain"
    ngx.say("Login token expired, please log in again")
    xtracer.LogXTrace("Bad Request - Expired Login TOken")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_OK)
  else
    local media_baggage = xtracer.BranchBaggage()
    local userid_baggage = xtracer.BranchBaggage()
    local text_baggage = xtracer.BranchBaggage()
    local uuid_baggage = xtracer.BranchBaggage()
    local baggages = {
      media_baggage,
      userid_baggage,
      text_baggage,
      uuid_baggage
    }
    local threads = {
      ngx.thread.spawn(_UploadMedia, req_id, post, carrier, media_baggage),
      ngx.thread.spawn(_UploadUserId, req_id, user_id, username, carrier, userid_baggage),
      ngx.thread.spawn(_UploadText, req_id, post, carrier, text_baggage),
      ngx.thread.spawn(_UploadUniqueId, req_id, post, carrier, uuid_baggage)
    }

    local status = ngx.HTTP_OK
    for i = 1, #threads do
      local ok, res = ngx.thread.wait(threads[i])
      if not ok then
        status = ngx.HTTP_INTERNAL_SERVER_ERROR
        xtracer.LogXTrace("Internal Server error")
        xtracer.DeleteBaggage()
      end
    end
    ngx.say("Successfully upload post")
    span:finish()
    xtracer.LogXTrace("Successfully uploaded post")
    ngx.exit(status)
    xtracer.DeleteBaggage()
  end



end

return _M
