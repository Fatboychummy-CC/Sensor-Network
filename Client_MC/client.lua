--[[
  This runs in ComputerCraft for minecraft. Running CC:Tweaked on MC 1.12.2.
  It may work on earlier versions, but not sure.

  This program was made quickly and probably badly, designed purely to aid in
  prototyping the server (ie: where information is sent to, and what the
  responses are).
]]

-- initialization
local expect = require("cc.expect").expect
-- get first terminal argument (default 'development')
local sEnvironment = select(1, ...) or "development"

-- paths for get/post
local paths = {
  alive = "alive",
  postAlarm = "detections/new"
}

-- determine environment connection info
local sProtocol = "http"
local sHttpServer = "##############"
local sPort = "8080" -- Don't change this VVV
if sEnvironment == "production" then
  sPort = "80" -- Change this one instead.
elseif sEnvironment ~= "development" then
  error("Unknown environment: " .. tostring(sEnvironment))
end

-- sensorTimeout is how long after a detection ends that the sensor is no longer
-- considered 'active'
local nSensorTimeout = 0.5

-- combine all server data into one.
local sServer = string.format("%s://%s:%s", sProtocol, sHttpServer, sPort)

-- headers to be used in transmissions
local tHeaders = {
  ["X-Clacks-Overhead"] = "GNU Terry Pratchet"
}
local tPostHeaders = {
  ["X-Clacks-Overhead"] = "GNU Terry Pratchet",
  ["Content-type"] = "application/json"
}

-- get info about the terminal and create a window for printing information.
local mX, mY = term.getSize()
local debugPrintWindow = window.create(term.current(), 1, 6, mX, mY - 6)

--[[
  printDebug [ vararg: any ]
  Redirects terminal output to the debug window, prints the information,
  then redirects back to whatever window was currently being used.
]]
local function printDebug(...)
  local oldTerm = term.redirect(debugPrintWindow)
  print(...)
  term.redirect(oldTerm)
end

--[[
  AddHeaders [ tAdd: dictionary of headers [ key:string, val:string ] ]
  Creates a clone of the headers table, adds whatever headers were passed
  to this function, then returns the new table.
]]
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

--[[
  AddPostHeaders [ tAdd: dictionary of headers [ key:string, val:string ] ]
  @AddHeaders
]]
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

--[[
  Post
    < sUri:string location on webserver to post to >,
    [ tData: dictionary of data to be converted to json and POSTed [ key:string, val:string ] ],
    [ tExtraHeaders: dictionary of headers [ key:string, val:string ] ]
  Sends a post request to the designated post receiver.
]]
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

--[[
  Get
    < sUri: string location on webserver to get from >,
    [ tExtraHeaders: dictionary of headers [key:string, val:string ] ]
  Make a get request to specified location on webserver.
]]
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
-- sensor environment init
local nLastSenseBot = os.clock() -- last clock cycle something was detected.
local nLastSenseTop = os.clock()
local bCat = true -- init as true to properly run.
local bTimeout = false -- timeout blocks repetition of alarms.
local bConnected = false -- just so it can print 'true' and look cool

--[[
  clearLine nil
  Clear the current line the cursor is occupying.
]]
local function clearLine()
  local x, y = term.getCursorPos()
  term.setCursorPos(1, y)
  term.write(string.rep(' ', mX))
  term.setCursorPos(x, y)
end

--[[
  drawDebug nil
  Draws a small bit of debug info just for niceness.
]]
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

--[[
  alarm nil
  Called when a cat is detected.
]]
local function alarm()
  Post(
    paths.postAlarm,
    {
      Sensor_Name = "Main Door",
      Time_Recorded = textutils.formatTime(os.time("utc") - 7)
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

--[[
  main nil
  The main loop of the program.
]]
local function main()
  -- initialize screen.
  term.clear()
  term.setCursorPos(1, 1)
  drawDebug()
  printDebug("Connecting...")

  -- create a connection. Server will respond from paths.alive stating it is
  -- alive.
  local h, err, bh = Get(paths.alive)
  if not h then
    -- if it failed, display error and info (if applicable), then stop.
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

  -- main loop
  while true do
    drawDebug() -- display small debug info.
    -- get redstone input ("sensor" input)
    local bTop, bBottom = rs.getInput("right"), not rs.getInput("bottom")

    -- if bottom is on, that means foot sensor active.
    if bBottom then
      nLastSenseBot = os.clock()
      bTimeout = false
      printDebug("Bottom triggered", nLastSenseBot)
    end

    -- if top is on, that means chest sensor active.
    if bTop then
      nLastSenseTop = os.clock()
      printDebug("Top triggered", nLastSenseTop)
    end

    if (bTop or nLastSenseTop + nSensorTimeout >= os.clock()) -- if top is on or not past top timeout (ie top was sensed before bottom)
       and nLastSenseBot + nSensorTimeout >= os.clock() then -- and bottom has timed out
      -- We know it is not a cat.
      bCat = false
      printDebug("Human.")
    end

    if (not bTimeout and nLastSenseBot + nSensorTimeout < os.clock()) then -- if we haven't already triggered a timeout, and we are overshooting the time
      -- if bottom sensor times out...
      bTimeout = true -- so we don't check again.

      if bCat then
        -- We know it is a cat! (Or we can make an educated guess, at least... It might be a dog too...)
        printDebug("Cat!!!")
        alarm()
      else
        printDebug("It's a human.")
      end

      -- reset this to true as we are using "it is a cat until proven otherwise" logic
      bCat = true
    end

    -- 20 checks per second (maximum limit of redstone changes in minecraft, as
    -- TPS (Ticks Per Second) maxes out at 20).
    os.sleep(0.05)
  end
end

--[[
  communicator nil
  Displays http responses.
]]
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

-- run main and communicator in parallel
parallel.waitForAny(main, communicator)
