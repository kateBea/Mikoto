function OnCreate()
	print("Hello from Player1")

	local tag = Entity:GetTag()
	local name = tag:GetTag()

	Console.Debug(string.format("My name tag is '%s'", name))
end

function OnUpdate(dt)
	local speed = 20.0
	local transform = Entity:GetTransform()
	local position = transform:GetTranslation()

	if Input.IsKeyPressed(KeyCode.Up) then
		print("Pressed Up")
		position.z = position.z - speed * dt

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

	transform:SetTranslation(position)
end