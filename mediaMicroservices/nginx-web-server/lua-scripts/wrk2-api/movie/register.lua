local _M = {}
local xtracer = require "luaxtrace"

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

function _M.RegisterMovie()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local GenericObjectPool = require "GenericObjectPool"
  local MovieIdServiceClient = require'media_service_MovieIdService'
  local ngx = ngx

  xtracer.StartLuaTrace("NginxWebServer", "RegisterMovie")
  xtracer.LogXTrace("Processing Request")

  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("RegisterMovie", {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  if (_StrIsEmpty(post.title) or _StrIsEmpty(post.movie_id)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtracer.LogXTrace("Incomplete arguments")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  carrier["baggage"] = xtracer.BranchBaggage()
  local client = GenericObjectPool:connection(MovieIdServiceClient,"movie-id-service",9090)

  local status = client:RegisterMovieId(req_id, post.title, tostring(post.movie_id), carrier)
  xtracer.JoinBaggage(status.baggage)
  GenericObjectPool:returnConnection(client)

  span:finish()
  xtracer.DeleteBaggage()
end

return _M
