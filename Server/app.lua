local lapis = require("lapis")
local app = lapis.Application()
local json_params = require("lapis.application").json_params

local alarmHistory = {n = 0}
function alarmHistory:add(t)
  self.n = self.n + 1
  self[self.n] = {
    Time = t.Time_Recorded,
    Name = t.Sensor_Name
  }
  self:det()
end
function alarmHistory:det()
  while self.n > 100 do
    table.remove(self, 1)
    self.n = self.n - 1
  end
end

app:get("/alive", function()
  return "true"
end)

app:get("/detections", function(self)
  return "Not yet implemented."
end)

app:post("/detections/new", json_params(function(self)
  if not self.params.Time_Recorded then
    ngx.print("Missing Time_Recorded parameter.")
    return {skip_render = true, status = 400}
  elseif not self.params.Sensor_Name then
    ngx.print("Missing Sensor_Name parameter.")
    return {skip_render = true, status = 400}
  end

  alarmHistory:add(self.params)

  ngx.print("Received.")
  return {skip_render = true}
end))

return app
