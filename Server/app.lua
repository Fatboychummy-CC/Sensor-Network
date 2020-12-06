local lapis = require("lapis")
local app = lapis.Application()
local json_params = require("lapis.application").json_params

app:get("/alive", function()
  return "true"
end)

app:post("/detections/new", json_params(function(self)
  ngx.print("Yeehaw bruv.")
  return {skip_render = true}
end))

return app
