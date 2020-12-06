local expect = require("cc.expect").expect
local tArgs = {...}
local sEnvironment = tArgs[1] or "development"
local sProtocol = "http"
local sHttpServer = "192.168.1.80"
local sPort = "8080"
if sEnvironment == "production" then
  sPort = "80"
elseif sEnvironment ~= "development" then
  error("Unknown environment: " .. tostring(sEnvironment))
end

local nSensorTimeout = 0.5
local paths = {
  alive = "alive",
  postAlarm = "detections/new"
}

local sServer = string.format("%s://%s:%s", sProtocol, sHttpServer, sPort)
local tHeaders = {
  ["X-Clacks-Overhead"] = "GNU Terry Pratchet"
}
local tPostHeaders = {
  ["X-Clacks-Overhead"] = "GNU Terry Pratchet",
  ["Content-type"] = "application/json"
}

local mX, mY = term.getSize()
local debugPrintWindow = window.create(term.current(), 1, 6, mX, mY - 6)

local function printDebug(...)
  local oldTerm = term.redirect(debugPrintWindow)
  print(...)
  term.redirect(oldTerm)
end

-- Create a table of headers containing the default headers and any we want to add.
local function AddHeaders(tAdd)
  expect(1, tAdd, "table", "nil")

  local tNewHeaders = {}
  for k, v in pairs(tHeaders) do
    tNewHeaders[k] = v
  end

  -- do the addition afterwards so we can overwrite defaults
  if type(tAdd) == "table" then
    for k, v in pairs(tAdd) do
      tNewHeaders[k] = v
    end
  end

  return tNewHeaders
end

local function AddPostHeaders(tAdd)
  expect(1, tAdd, "table", "nil")

  local tNewHeaders = {}
  for k, v in pairs(tPostHeaders) do
    tNewHeaders[k] = v
  end

  -- do the addition afterwards so we can overwrite defaults
  if type(tAdd) == "table" then
    for k, v in pairs(tAdd) do
      tNewHeaders[k] = v
    end
  end

  return tNewHeaders
end

-- Post data to webserver.
local function Post(sUri, tData, tExtraHeaders)
  expect(1, sUri, "string", "nil")
  expect(2, tData, "table", "string")
  expect(3, tExtraHeaders, "table", "nil")
  printDebug(textutils.serialiseJSON(tData or true))

  return http.request(
    string.format("%s/%s", sServer, sUri or ""),
    textutils.serialiseJSON(tData or true), -- post data
    AddPostHeaders(tExtraHeaders) -- headers
  )
end

-- Get data from webserver
local function Get(sUri, tExtraHeaders)
  expect(1, sUri, "string", "nil")
  expect(2, tExtraHeaders, "table", "nil")

  return http.get(
    string.format("%s/%s", sServer, sUri or ""),
    AddHeaders(tExtraHeaders)
  )
end

--########################################
-- start main
--########################################
local nLastSenseBot = os.clock()
local nLastSenseTop = os.clock()
local bCat = true
local bTimeout = false
local bConnected = false

local function clearLine()
  local x, y = term.getCursorPos()
  term.setCursorPos(1, y)
  term.write(string.rep(' ', mX))
  term.setCursorPos(x, y)
end

local function drawDebug()
  term.setTextColor(colors.white)

  term.setCursorPos(1, 1)
  clearLine()
  term.write(string.format("SERVER: %s", sServer))

  term.setCursorPos(1, 2)
  clearLine()
  term.write(string.format("CONNECTED: %s", tostring(bConnected)))

  term.setCursorPos(1, 5)
  term.setBackgroundColor(colors.gray)
  clearLine()
  term.setBackgroundColor(colors.black)
end

local function alarm()
  Post(
    paths.postAlarm,
    {
      Sensor_Name = "Main Door",
      Time_Recorded = os.clock()
    }
  )
  printDebug("Sent POST with detection info.")
  rs.setOutput("top", false)
  for i = 1, 8 do
    rs.setOutput("top", not rs.getOutput("top"))
    os.sleep(0.5)
  end
  rs.setOutput("top", false)
end

local function main()
  term.clear()
  term.setCursorPos(1, 1)
  drawDebug()
  printDebug("Connecting...")
  local h, err, bh = Get(paths.alive)
  if not h then
    printError("Failed to connect.")
    local nStatus = -1
    if bh then
      nStatus = bh.getResponseCode()
      bh.close()
    end
    error(string.format("%d: %s", nStatus, err), -1)
  end
  bConnected = true
  printDebug("Server is alive!")

  os.startTimer(1) -- ensure timer index is not at 0.

  while true do
    drawDebug()
    local bTop, bBottom = rs.getInput("right"), not rs.getInput("bottom")

    if bBottom then
      nLastSenseBot = os.clock()
      bTimeout = false
      printDebug("Bottom triggered", nLastSenseBot)
    end

    if bTop then
      nLastSenseTop = os.clock()
      printDebug("Top triggered", nLastSenseTop)
    end

    if (bTop or nLastSenseTop + nSensorTimeout >= os.clock()) -- if top is on or not past top timeout (ie top was sensed before bottom)
       and nLastSenseBot + nSensorTimeout >= os.clock() then -- and bottom has timed out
      -- human.
      bCat = false
      printDebug("Human.")
    end

    if (not bTimeout and nLastSenseBot + nSensorTimeout < os.clock()) then -- if we haven't already triggered a timeout, and we are overshooting the time
      bTimeout = true -- so we don't check again.
      if bCat then
        printDebug("Cat!!!")
        alarm()
      else
        printDebug("It's a human.")
      end
      bCat = true
    end
    os.sleep(0.05)
  end
end

local function communicator()
  while true do
    local ev = table.pack(os.pullEvent())
    if ev[1] == "http_success" or ev[1] == "http_failure" then
      printDebug("###")
      printDebug(ev[2])
      printDebug(ev[3].readAll())
      printDebug("###")
    end
  end
end

parallel.waitForAny(main, communicator)
