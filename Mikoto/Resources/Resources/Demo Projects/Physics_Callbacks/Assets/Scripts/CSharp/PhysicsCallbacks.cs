using System;
using System.Collections.Generic;
using Mikoto;

// Focus Game after Play. The same script observes both sides of each interaction.
// Update/Stay output is throttled; the engine still delivers every callback.
public sealed class PhysicsCallbacks : PlaygroundCharacterController
{
    [SerializeField] public string Label = "Actor";
    [SerializeField] public bool IsPlayer;

    private CharacterControllerComponent? _character;
    private float _clock;
    private float _nextUpdateLog = 1;
    private readonly Dictionary<(bool Trigger, ulong Other), float> _nextStayLog = new();

    public override void OnCreate()
    {
        if (IsPlayer)
        {
            base.OnCreate();
            _character = Entity.GetComponent<CharacterControllerComponent>();
            Log.Info("Physics callbacks: focus Game, WASD moves, Space jumps, R resets. Walk into both colored trigger zones and touch the solid blocks.");
        }
        Log.Info($"[{Label}] OnCreate ({EntityId})");
    }

    public override void OnUpdate(float timeStep)
    {
        _clock += timeStep;
        if (_character == null)
        {
            return;
        }
        base.OnUpdate(timeStep);
        if (_clock >= _nextUpdateLog)
        {
            Log.Info($"[{Label}] OnUpdate / grounded: {_character.IsGrounded}");
            _nextUpdateLog = _clock + 1;
        }
    }

    public override void OnCollisionEnter(Entity other)
    {
        Report("OnCollisionEnter", other);
    }

    public override void OnCollisionStay(Entity other)
    {
        ReportStay("OnCollisionStay", other, false);
    }

    public override void OnCollisionExit(Entity other)
    {
        _nextStayLog.Remove((false, other.Id));
        Report("OnCollisionExit", other);
    }

    public override void OnTriggerEnter(Entity other)
    {
        Report("OnTriggerEnter", other);
    }

    public override void OnTriggerStay(Entity other)
    {
        ReportStay("OnTriggerStay", other, true);
    }

    public override void OnTriggerExit(Entity other)
    {
        _nextStayLog.Remove((true, other.Id));
        Report("OnTriggerExit", other);
    }

    public override void OnDestroy()
    {
        Log.Info($"[{Label}] OnDestroy ({EntityId})");
    }

    private void Report(string callback, Entity other)
    {
        Log.Info($"[{Label}] {callback} / other entity: {other.Id}");
    }

    private void ReportStay(string callback, Entity other, bool trigger)
    {
        var key = (trigger, other.Id);
        if (!_nextStayLog.TryGetValue(key, out float next) || _clock >= next)
        {
            Report(callback, other);
            _nextStayLog[key] = _clock + 1;
        }
    }
}
