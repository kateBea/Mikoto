local enable = false

function VectorMathTest()
    print("=== GLM VECTOR TEST BEGIN ===")

    -- Vec2
    local v2a = Vec2F.new(1.0, 2.0)
    local v2b = Vec2F.new(3.0, 4.0)
    local v2c = v2a + v2b
    local v2d = v2c * 2.0

    print("Vec2:")
    print("  v2a =", v2a)
    print("  v2b =", v2b)
    print("  v2c = v2a + v2b =", v2c)
    print("  v2d = v2c * 2 =", v2d)

    -- Vec3
    local v3a = Vec3F.new(1.0, 2.0, 3.0)
    local v3b = Vec3F.new(4.0, 5.0, 6.0)
    local v3c = v3a + v3b
    local v3d = v3c * 0.5
    local v3e = -v3a

    print("Vec3:")
    print("  v3a =", v3a)
    print("  v3b =", v3b)
    print("  v3c = v3a + v3b =", v3c)
    print("  v3d = v3c * 0.5 =", v3d)
    print("  v3e = -v3a =", v3e)

    -- Vec4
    local v4a = Vec4F.new(1.0, 2.0, 3.0, 4.0)
    local v4b = Vec4F.new(5.0, 6.0, 7.0, 8.0)
    local v4c = v4a + v4b
    local v4d = v4c / 2.0

    print("Vec4:")
    print("  v4a =", v4a)
    print("  v4b =", v4b)
    print("  v4c = v4a + v4b =", v4c)
    print("  v4d = v4c / 2 =", v4d)

    -- Field access test
    print("Field access:")
    print(string.format(
        "  v4d = (%.2f, %.2f, %.2f, %.2f)",
        v4d.x, v4d.y, v4d.z, v4d.w
    ))

    -- Equality test
    local v3f = Vec3F.new (1.0, 2.0, 3.0)
    print("Equality test (v3a == v3f):", v3a == v3f)

    print("=== GLM VECTOR TEST END ===")
end

function OnCreate()
    print("Hello")

    VectorMathTest()
end

function OnUpdate(dt)
    local speed = 20.0
    local transform = Entity:GetTransform()
    local position = transform:GetTranslation()


    if Input.IsKeyPressed(KeyCode.Space) then
        print("Pressed Space")
        enable = not enable
        return
    end

    if Input.IsKeyPressed(KeyCode.Up) then
        print("Pressed Up")
        position.z = position.z - speed * dt

        Console.Debug("You pressed Up key")
    end

    if Input.IsKeyPressed(KeyCode.Down) then
        print("Pressed Down")
        position.z = position.z + speed * dt
    end

    if Input.IsKeyPressed(KeyCode.Left) then
        print("Pressed Left")
        position.x = position.x - speed * dt
    end

    if Input.IsKeyPressed(KeyCode.Right) then
        print("Pressed Right")
        position.x = position.x + speed * dt
    end

    --transform:SetTranslation(position)
end