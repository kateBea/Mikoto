-- hello.lua
-- Simple test script for Mikoto’s Lua console

local t = 0.0

print("Hello from Lua! Script started.")

-- Simulate an update loop
for i = 1, 5 do
	t = t + 0.5
	print(string.format("[Frame %d] Time = %.2f seconds", i, t))
end

print("Lua script finished successfully.")
