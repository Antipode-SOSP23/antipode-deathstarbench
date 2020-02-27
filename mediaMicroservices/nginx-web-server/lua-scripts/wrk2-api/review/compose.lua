local _M = {}
local xtracer = require "luaxtrace"

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

local function _UploadUserId(req_id, post, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local UserServiceClient = require 'media_service_UserService'
  carrier["baggage"] = xtracer.BranchBaggage()
  local user_client = GenericObjectPool:connection(
    UserServiceClient,"user-service",9090)
  local status = user_client:UploadUserWithUsername(req_id, post.username, carrier)
  print("Status is:", status)
  xtracer.JoinBaggage(status.baggage)
  GenericObjectPool:returnConnection(user_client)
end

local function _UploadText(req_id, post, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local TextServiceClient = require 'media_service_TextService'
  carrier["baggage"] = xtracer.BranchBaggage()
  local text_client = GenericObjectPool:connection(
    TextServiceClient,"text-service",9090)
  local status = text_client:UploadText(req_id, post.text, carrier)
  print("Status is:", status)
  xtracer.JoinBaggage(status.baggage)
  GenericObjectPool:returnConnection(text_client)
end

local function _UploadMovieId(req_id, post, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local MovieIdServiceClient = require 'media_service_MovieIdService'
  carrier["baggage"] = xtracer.BranchBaggage()
  local movie_id_client = GenericObjectPool:connection(
    MovieIdServiceClient,"movie-id-service",9090)
  local status = movie_id_client:UploadMovieId(req_id, post.title, tonumber(post.rating), carrier)
  print("Status is:", status)
  xtracer.JoinBaggage(status.baggage)
  GenericObjectPool:returnConnection(movie_id_client)
end

local function _UploadUniqueId(req_id, carrier, baggage)
  xtracer.SetBaggage(baggage)
  local GenericObjectPool = require "GenericObjectPool"
  local UniqueIdServiceClient = require 'media_service_UniqueIdService'
  carrier["baggage"] = xtracer.BranchBaggage()
  local unique_id_client = GenericObjectPool:connection(
    UniqueIdServiceClient,"unique-id-service",9090)
  status = unique_id_client:UploadUniqueId(req_id, carrier)
  print("Status is:", status)
  xtracer.JoinBaggage(status.baggage)
  GenericObjectPool:returnConnection(unique_id_client)
end

function _M.ComposeReview()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local ngx = ngx

  xtracer.StartLuaTrace("NginxWebServer5", "ComposeReview");
  xtracer.LogXTrace("Processing Request")
  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("ComposeReview", {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  if (_StrIsEmpty(post.title) or _StrIsEmpty(post.text) or
      _StrIsEmpty(post.username) or _StrIsEmpty(post.password) or
      _StrIsEmpty(post.rating)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  local userid_baggage = xtracer.BranchBaggage()
  local movieid_baggage = xtracer.BranchBaggage()
  local text_baggage = xtracer.BranchBaggage()
  local uuid_baggage = xtracer.BranchBaggage()
  local threads = {
    ngx.thread.spawn(_UploadUserId, req_id, post, carrier, userid_baggage),
    ngx.thread.spawn(_UploadMovieId, req_id, post, carrier, movieid_baggage),
    ngx.thread.spawn(_UploadText, req_id, post, carrier, text_baggage),
    ngx.thread.spawn(_UploadUniqueId, req_id, carrier, uuid_baggage)
  }

  local baggages = {
    userid_baggage,
    movieid_baggage,
    text_baggage,
    uuid_baggage
  }

  local status = ngx.HTTP_OK
  for i = 1, #threads do
    local ok, res = ngx.thread.wait(threads[i])
    xtracer.JoinBaggage(baggages[i])
    if not ok then
      status = ngx.HTTP_INTERNAL_SERVER_ERROR
      xtracer.LogXTrace("Internal Server error")
    end
  end
  span:finish()
  xtracer.LogXTrace("Request processing finished")
  ngx.exit(status)
  xtracer.DeleteBaggage()
  
end

return _M
