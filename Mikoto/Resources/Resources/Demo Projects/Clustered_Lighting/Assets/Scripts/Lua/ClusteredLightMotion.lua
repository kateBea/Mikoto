local ClusteredLightMotion = {}
ClusteredLightMotion.Properties = {}

-- Attach to a point or spot light. Play runs scripts; Simulate runs physics only.
-- Each script instance owns its locals, so lights retain independent motion.
-- Edit these defaults and restart Play. Distances are world units; angles are radians.
local motionSpeed = 0.8
local orbitRadius = 0.22
local bobHeight = 0.15
local pulseAmount = 0.3

local elapsed = 0.0
local transform
local light
local spot
local origin
local phase
local angularSpeed
local initialOffset
local initialIntensity
local initialRadius

function ClusteredLightMotion:OnCreate()
    -- Resolve components once rather than repeat ECS lookups for 15,000 lights each frame.
    transform = self.Entity:GetTransform()
    spot = self.Entity:GetSpotLight()
    light = spot or self.Entity:GetPointLight()
    origin = transform:GetTranslation()

    -- Position supplies a repeatable phase and speed, independent of runtime entity IDs.
    phase = origin.x * 1.37 + origin.z * 0.73 + origin.y * 0.31
    angularSpeed = motionSpeed * (0.75 + 0.25 * math.sin(phase * 2.1))
    initialOffset = Vec3F.new(math.cos(phase), math.sin(phase * 1.3), math.sin(phase))

    if light ~= nil then
        initialIntensity = light:GetIntensity()
        initialRadius = light:GetRadius()
    end
end

function ClusteredLightMotion:OnUpdate(deltaTime)
    if deltaTime ~= deltaTime or deltaTime < 0.0 or deltaTime == math.huge then
        return
    end

    if light == nil or initialIntensity == nil then
        return
    end

    elapsed = elapsed + deltaTime
    local angle = elapsed * angularSpeed + phase

    -- Subtract the initial offset to start at the authored position without a snap.
    -- Maximum excursion is twice each amplitude: 0.44 in X/Z and 0.30 in Y.
    transform:SetTranslation(Vec3F.new(
        origin.x + orbitRadius * (math.cos(angle) - initialOffset.x),
        origin.y + bobHeight * (math.sin(angle * 1.3) - initialOffset.y),
        origin.z + orbitRadius * (math.sin(angle) - initialOffset.z)))
    light:SetIntensity(initialIntensity * (1.0 - pulseAmount * 0.5 + pulseAmount * 0.5 * math.sin(angle * 1.7)))

    -- Varying bounds tests culling updates as well as moving light centers.
    light:SetRadius(initialRadius * (0.9 + 0.1 * math.sin(angle * 0.7)))

    if spot ~= nil then
        -- Sweep cones while keeping their main direction aimed toward the ground.
        spot:SetDirection(Vec3F.new(0.45 * math.sin(angle), -1.0, 0.45 * math.cos(angle)))
    end
end

return ClusteredLightMotion
