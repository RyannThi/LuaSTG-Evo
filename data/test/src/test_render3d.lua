local test = require("test")

local function Camera3D()
    ---@class kuanlan.Camera3D
    local M = {
        dir = 90,
        upd = 0,
        x = 0,
        y = 0,
        z = -10,
        speed = 0.05,

        rbtn = false,
        mx = 0,
        my = 0,
        last_dir = 90,
        last_upd = 0,
        aspeed = 0.1,

        fog_near = 5,
        for_far = 20,
    }
    function M:update()
        local speed = self.speed
        local Key = lstg.Input.Keyboard
        if lstg.GetKeyState(Key.LeftControl) then
            speed = speed * 10.0
        end
        if lstg.GetKeyState(Key.W) then
            self.x = self.x + speed * math.cos(math.rad(self.dir))
            self.z = self.z + speed * math.sin(math.rad(self.dir))
        elseif lstg.GetKeyState(Key.S) then
            self.x = self.x - speed * math.cos(math.rad(self.dir))
            self.z = self.z - speed * math.sin(math.rad(self.dir))
        end
        if lstg.GetKeyState(Key.D) then
            self.x = self.x + speed * math.cos(math.rad(self.dir - 90))
            self.z = self.z + speed * math.sin(math.rad(self.dir - 90))
        elseif lstg.GetKeyState(Key.A) then
            self.x = self.x + speed * math.cos(math.rad(self.dir + 90))
            self.z = self.z + speed * math.sin(math.rad(self.dir + 90))
        end
        if lstg.GetKeyState(Key.Space) then
            self.y = self.y + speed
        elseif lstg.GetKeyState(Key.LeftShift) then
            self.y = self.y - speed
        end
        if not self.rbtn and lstg.GetMouseState(2) then
            self.rbtn = true
            self.mx, self.my = lstg.GetMousePosition()
            self.last_dir = self.dir
            self.last_upd = self.upd
        elseif self.rbtn and not lstg.GetMouseState(2) then
            self.rbtn = false
        end
        if self.rbtn then
            local mx, my = lstg.GetMousePosition()
            local dx, dy = mx - self.mx, my - self.my
            self.dir = self.last_dir - self.aspeed * dx
            self.upd = self.last_upd + self.aspeed * dy
            self.upd = math.max(-89.0, math.min(self.upd, 89.0))
        end
    end
    function M:apply()
        local tx, ty, tz = 1, 0, 0
        local ux, uy, uz = 0, 1, 0
        local function vec2_rot(x, y, r_deg)
            local sin_v = math.sin(math.rad(r_deg))
            local cos_v = math.cos(math.rad(r_deg))
            return x * cos_v - y * sin_v, x * sin_v + y * cos_v
        end
        tx, ty = vec2_rot(tx, ty, self.upd)
        ux, uy = vec2_rot(ux, uy, self.upd)
        tx, tz = vec2_rot(tx, tz, self.dir)
        ux, uz = vec2_rot(ux, uz, self.dir)
        lstg.SetViewport(0, window.width, 0, window.height)
        lstg.SetScissorRect(0, window.width, 0, window.height)
        lstg.SetPerspective(
            self.x, self.y, self.z,
            self.x + tx, self.y + ty, self.z + tz,
            ux, uy, uz,
            math.rad(80), window.width / window.height,
            0.01, 1000.0
        )
        lstg.SetImageScale(1)
        lstg.SetFog()
        --lstg.SetFog(self.fog_near, self.for_far, lstg.Color(255, 255, 255, 255))
    end
    return M
end

local camera3d = Camera3D()

local function load_texture(f)
    lstg.LoadTexture("tex:" .. f, "res/" .. f, false)
    local w, h = lstg.GetTextureSize("tex:" .. f)
    lstg.LoadImage("img:" .. f, "tex:" .. f, 0, 0, w, h)
end
local function unload_texture(f)
    lstg.RemoveResource("global", 2, "img:" .. f)
    lstg.RemoveResource("global", 1, "tex:" .. f)
end

---@class test.Module.Texture : test.Base
local M = {}

function M:onCreate()
    local old_pool = lstg.GetResourceStatus()
    lstg.SetResourceStatus("global")

    -- load_texture("sRGB.png")
    -- load_texture("linear.png")
    load_texture("block.png")
    -- load_texture("block.qoi")

    lstg.SetResourceStatus(old_pool)
end

function M:onDestroy()
    -- unload_texture("sRGB.png")
    -- unload_texture("linear.png")
    unload_texture("block.png")
    -- unload_texture("block.qoi")
end

function M:onUpdate()
    camera3d:update()
end

local timer = 0
function M:onRender()
    camera3d:apply()

    -- window:applyCameraV()
    local scale = 0.01
    scale = 0.01
    lstg.Render3D('img:block.png', 0, 0, 0, timer / 3, timer / 2, timer, scale, scale)
    timer = timer + 1
end

test.registerTest("test.Module.Render3D", M)
