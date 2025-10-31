-- terminal_adventure.lua
-- A small Lua console demo that simulates a text-based adventure

-- Utility function to simulate typing effect
local function type_out(text, delay)
	for i = 1, #text do
		io.write(text:sub(i, i))
		io.flush()
		os.execute("sleep " .. (delay or 0.02))
	end
	print()
end

-- Table describing a simple player
local Player = {
	name = "Adventurer",
	health = 100,
	inventory = {}
}

local function print_status()
	print(string.rep("-", 40))
	print(string.format("Player: %s | Health: %d", Player.name, Player.health))
	print("Inventory:", table.concat(Player.inventory, ", "))
	print(string.rep("-", 40))
end

-- Add an item to the inventory
local function add_item(item)
	table.insert(Player.inventory, item)
	type_out("You picked up a " .. item .. ".", 0.01)
end

-- Simulate a small encounter
local function encounter_enemy(enemy, damage)
	type_out("A wild " .. enemy .. " appears!", 0.03)
	Player.health = Player.health - damage
	type_out("The " .. enemy .. " attacks you for " .. damage .. " damage!", 0.03)
	if Player.health <= 0 then
		type_out("You have been defeated...", 0.05)
		os.exit()
	else
		type_out("You barely survive and escape!", 0.03)
	end
end

-- Game starts here
type_out("Welcome to the Lua Terminal Adventure!", 0.04)
os.execute("sleep 0.5")

print_status()
os.execute("sleep 0.5")

add_item("Torch")
add_item("Old Key")

encounter_enemy("Goblin", 15)
print_status()

type_out("You find a locked door. The key fits perfectly.", 0.03)
type_out("Inside the room, you see a chest filled with gold!", 0.03)
add_item("Gold Coins")

print_status()
type_out("Congratulations! You survived your first adventure.", 0.04)

print("\n--- End of Script ---")
