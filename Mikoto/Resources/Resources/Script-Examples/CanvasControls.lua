local CanvasControls = {}
CanvasControls.Properties = {}

-- Attach to the entity owning the Canvas component. In its inspector, add:
-- Button named "Action", Toggle named "Details", Slider named "Volume",
-- and Text named "Status". A panel can own these widgets as children.
-- Names bind once; the resulting IDs survive widget reordering.
local canvas
local action, details, volume, status

function CanvasControls:OnCreate()
    canvas = Entity:GetCanvas()
    if canvas == nil then return end
    action = canvas:FindElement("Action")
    details = canvas:FindElement("Details")
    volume = canvas:FindElement("Volume")
    status = canvas:FindElement("Status")
end

function CanvasControls:OnUpdate(dt)
    if canvas == nil then return end
    if canvas:ConsumeElementClick(action) then
        canvas:SetElementText(status, "Button activated")
    end
    if canvas:ConsumeValueChange(details) then
        canvas:SetElementVisible(status, canvas:GetElementValue(details) > 0.5)
    end
    if canvas:ConsumeValueChange(volume) then
        canvas:SetElementText(status, string.format("Volume: %.0f%%", canvas:GetElementValue(volume) * 100))
    end
end

return CanvasControls
