local _M = {}
local xtracer = require "luaxtrace"

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

function _M.Follow()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local ngx = ngx
  local GenericObjectPool = require "GenericObjectPool"
  local SocialGraphServiceClient = require "social_network_SocialGraphService"

  xtracer.StartLuaTrace("NginxWebServer", "Follow")
  xtracer.LogXTrace("Processing Request")
  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(
      ngx.var.opentracing_binary_context)
  local span = tracer:start_span("Follow",
      {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  local client = GenericObjectPool:connection(
      SocialGraphServiceClient, "social-graph-service", 9090)

  local status
  local err
  carrier["baggage"] = xtracer.BranchBaggage()
  if (not _StrIsEmpty(post.user_id) and not _StrIsEmpty(post.followee_id)) then
    xtracer.LogXTrace("Trying to follow using user-id")
    status, err = pcall(client.Follow, client,req_id,
        tonumber(post.user_id), tonumber(post.followee_id), carrier )
  elseif (not _StrIsEmpty(post.user_name) and not _StrIsEmpty(post.followee_name)) then
    xtracer.LogXTrace("Trying to follow using username")
    status, err = pcall(client.FollowWithUsername, client,req_id,
        post.user_name, post.followee_name, carrier )
  else
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtrace.LogXTrace("Incomplete arguments")
    xtrace.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  if not status then
    ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
    ngx.say("Follow Failed: " .. err.message)
    ngx.log(ngx.ERR, "Follow Failed: " .. err.message)
    xtrace.LogXTrace("Follow Failed: " .. err.message)
    xtrace.DeleteBaggage()
    ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
  end
  GenericObjectPool:returnConnection(client)
  span:finish()

  xtrace.DeleteBaggage()
end

return _M
