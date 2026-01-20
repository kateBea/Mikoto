function OnCreate()
    print("Hello from Player 3 GAROO")

    local tag = Entity:GetTag()
    local name = tag:GetTag()

    Console.Debug(string.format("My name tag is '%s'", name))
    Console.Warn(string.format("My name tag is '%s'", name))
    Console.Error(string.format("My name tag is '%s'", name))
end

function OnUpdate(dt)
    local speed = 20.0
    local transform = Entity:GetTransform()
    local position = transform:GetTranslation()

    if Input.IsKeyPressed(KeyCode.T) then
        print("Pressed T")
        position.z = position.z - speed * dt

    end

    if Input.IsKeyPressed(KeyCode.G) then
        print("Pressed G")
        position.z = position.z + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.F) then
        print("Pressed F")
        position.x = position.x - speed * dt
    end

    if Input.IsKeyPressed(KeyCode.H) then
        print("Pressed H")
        position.x = position.x + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.Space) then
        print("Pressed Space")
        position.y = position.y + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.X) then
        print("Pressed X")
        position.y = position.y - speed * dt
    end

    transform:SetTranslation(position)
end