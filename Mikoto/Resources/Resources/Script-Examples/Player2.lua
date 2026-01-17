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

    if Input.IsKeyPressed(KeyCode.W) then
        print("Pressed W")
        position.z = position.z - speed * dt

        Console.Debug("You pressed Up key")
    end

    if Input.IsKeyPressed(KeyCode.S) then
        print("Pressed S")
        position.z = position.z + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.A) then
        print("Pressed A")
        position.x = position.x - speed * dt
    end

    if Input.IsKeyPressed(KeyCode.D) then
        print("Pressed D")
        position.x = position.x + speed * dt
    end

    transform:SetTranslation(position)
end