local _M = {}
local xtracer = require "luaxtrace"

function _M.WritePlot()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local GenericObjectPool = require "GenericObjectPool"
  local PlotServiceClient = require 'media_service_PlotService'
  local ngx = ngx
  local cjson = require("cjson")

  xtracer.StartLuaTrace("NginxWebServer", "WritePlot")
  xtracer.LogXTrace("Processing Request")

  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("WritePlot  ", {["references"] = {{"child_of", parent_span_context}}})
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

  local plot = cjson.decode(data)
  if (plot["plot_id"] == nil or plot["plot"] == nil) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtracer.LogXTrace("Incomplete arguments")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  carrier["baggage"] = xtracer.BranchBaggage()
  local client = GenericObjectPool:connection(PlotServiceClient, "plot-service", 9090)
  local status, err = client:WritePlot(req_id, plot["plot_id"], plot["plot"], carrier)
  xtracer.JoinBaggage(err.baggage)
  GenericObjectPool:returnConnection(client)
  xtracer.DeleteBaggage()
end

return _M
