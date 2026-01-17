function OnCreate()
    print("Hello from Player2")

    local tag = Entity:GetTag()
    local name = tag:GetTag()

    Console.Debug(string.format("My name tag is '%s'", name))
end

function OnUpdate(dt)
    local speed = 20.0
    local transform = Entity:GetTransform()
    local position = transform:GetTranslation()

    if Input.IsKeyPressed(KeyCode.I) then
        print("Pressed I")
        position.z = position.z - speed * dt

    end

    if Input.IsKeyPressed(KeyCode.K) then
        print("Pressed K")
        position.z = position.z + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.J) then
        print("Pressed J")
        position.x = position.x - speed * dt
    end

    if Input.IsKeyPressed(KeyCode.L) then
        print("Pressed L")
        position.x = position.x + speed * dt
    end

    transform:SetTranslation(position)
end