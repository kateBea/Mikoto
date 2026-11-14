local AudioOnCreate = {}

AudioOnCreate.Properties = {
    Clip = { Type = "Asset", Kind = "Audio", Default = nil }
}

function AudioOnCreate:OnCreate()
    -- Assign Clip in the inspector and add an AudioSourceComponent to this entity.
    if self.Clip then
        self.Clip:Play(self.Entity)
    end
end

return AudioOnCreate
