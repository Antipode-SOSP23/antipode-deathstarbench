local _M = {}
local xtracer = require "luaxtrace"

function _M.WriteCastInfo()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local GenericObjectPool = require "GenericObjectPool"
  local CastInfoServiceClient = require 'media_service_CastInfoService'
  local ngx = ngx

  xtracer.StartLuaTrace("NginxWebServer", "WriteCastInfo")
  xtracer.LogXTrace("Processing Request")

  local cjson = require("cjson")

  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("WriteCastInfo  ", {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local data = ngx.req.get_body_data()

  if not data then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Empty body")
    ngx.log(ngx.ERR, "Empty body")
    xtracer.LogXTrace("Empty body")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  local cast_info = cjson.decode(data)
  if (cast_info["cast_info_id"] == nil or cast_info["name"] == nil or
      cast_info["gender"] == nil or cast_info["intro"] == nil) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtracer.LogXTrace("Incomplete arguments")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  carrier["baggage"] = xtracer.BranchBaggage()
  local client = GenericObjectPool:connection(CastInfoServiceClient, "cast-info-service", 9090)
  local status, err = client:WriteCastInfo(req_id, cast_info["cast_info_id"], cast_info["name"],
      cast_info["gender"], cast_info["intro"],  carrier)
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(client)
  xtracer.DeleteBaggage()
end

return _M
