function OnCreate()
    local tag = Entity:GetTag()
    local name = tag:GetTag()

    Console.Debug(string.format("My name is '%s'", name))
end

function OnUpdate(dt)
    
end