local RotateY = {}
RotateY.Properties = {
    Speed = { Type = "Float", Default = 90.0 }
}

function RotateY:OnCreate()
end

function RotateY:OnUpdate(timeStep)
    -- Speed is an editor property, applied before the first callback.
    local transform = self.Entity:GetComponent("TransformComponent")
    local rotation = transform:GetRotation()
    rotation.y = (rotation.y + self.Speed * timeStep) % 360.0
    transform:SetRotation(rotation)
end

function RotateY:OnDestroy()
end

return RotateY
