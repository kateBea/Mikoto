local StarterCubesController = {}
StarterCubesController.Properties = {}

-- A small rigid-body playground, not a character controller.
-- IJKL: move the cubes on the ground plane. U: give them an upward kick.
-- R: reset the box. Stop Play restores the original editor scene.
-- Tune these values, save this file, then restart Play to try your changes.
local moveSpeed = 4.0
local acceleration = 18.0
local kickSpeed = 6.0
local spawnPosition
local spawnRotation
local kickWasDown = false
local resetWasDown = false

function StarterCubesController:OnCreate()
    spawnPosition = Entity:GetTransform():GetTranslation()
    spawnRotation = Entity:GetTransform():GetRotation()
    Console.Info("Starter cubes: IJKL moves all four cubes, U tosses them, R resets them. WASD / Space controls the animated character.")
end

function StarterCubesController:OnUpdate(dt)
    local body = Entity:GetRigidBody()
    if body == nil then
        return
    end

    local transform = Entity:GetTransform()
    local resetDown = Input.IsKeyPressed(KeyCode.R)
    local kickDown = Input.IsKeyPressed(KeyCode.U)
    if (resetDown and not resetWasDown) or transform:GetTranslation().y < -15.0 then
        transform:SetRotation(spawnRotation)
        transform:SetTranslation(spawnPosition)
        body:SetLinearVelocity(Vec3F.new(0.0, 0.0, 0.0))
        body:SetAngularVelocity(Vec3F.new(0.0, 0.0, 0.0))
        resetWasDown = resetDown
        kickWasDown = kickDown
        return
    end
    resetWasDown = resetDown

    local x, z = 0.0, 0.0
    if Input.IsKeyPressed(KeyCode.J) then
        x = x - 1.0
    end
    if Input.IsKeyPressed(KeyCode.L) then
        x = x + 1.0
    end
    if Input.IsKeyPressed(KeyCode.I) then
        z = z - 1.0
    end
    if Input.IsKeyPressed(KeyCode.K) then
        z = z + 1.0
    end
    -- Normalize diagonals so I+L does not move faster than I alone.
    local length = math.sqrt(x * x + z * z)
    if length > 0.0 then
        x, z = x / length, z / length
    end

    local velocity = body:GetLinearVelocity()
    local blend = math.min(math.max(dt, 0.0) * acceleration, 1.0)
    velocity.x = velocity.x + (x * moveSpeed - velocity.x) * blend
    velocity.z = velocity.z + (z * moveSpeed - velocity.z) * blend
    -- Preserve vertical velocity so gravity and collision response still work.
    -- Edge detection makes U a single kick, not continuous flight.
    if kickDown and not kickWasDown then
        velocity.y = kickSpeed
    end
    kickWasDown = kickDown
    body:SetLinearVelocity(velocity)
end

return StarterCubesController
