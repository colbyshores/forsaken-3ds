#!/usr/bin/env python3
"""
Parse Forsaken .TRG binary trigger files.

Format (from triggers.c TriggerLoad()):
  Header (5 × int32):
    NumOfTrigVars, NumOfTriggers, NumOfTrigMods, NumOfEvents, NumOfConditions

  TrigVars[NumOfTrigVars]:
    char Name[32]
    int32 InitState
    int32 NumOfTriggers
    int32 TriggerIndices[NumOfTriggers]

  Triggers[NumOfTriggers]:
    uint8  Type
    int32  TrigVarIndex
    int32  ActiveState
    int32  NumOfConditions
    int32  ConditionIndices[NumOfConditions]

  TrigMods[NumOfTrigMods]:
    int32  Op
    int32  TrigVarIndex
    int32  Val
    float  Time
    int32  NextIndex (-1 = None)

  Events[NumOfEvents]:   (byte-stream, no padding)
    uint8  Type
    uint8  Data[64]

  Conditions[NumOfConditions]:  (back to int32-aligned)
    int32  NumOfTriggers
    int32  TriggerIndices[NumOfTriggers]
    int32  NumOfEvents
    int32  EventIndices[NumOfEvents]

Event type names (from triggers.c EventList[]):
  0  PickupGenerate
  1  EnemyGenerate
  2  OpenDoor
  3  CloseDoor
  4  LockDoor
  5  UnlockDoor
  6  StartBGOAnim
  7  StopBGOAnim
  8  StartTexAnim
  9  StopTexAnim
 10  ModifyTrigVar
 11  TriggerMod
 12  (reserved)
 13  WaterFill
 14  (reserved -- WaterDrain in source uses index 15)
 15  WaterDrain
 16  StartSpotFX
 17  StopSpotFX
 18  ShowMessages
 19  EnableLight
 20  DisableLight
 21  TriggerAreaEnable
 22  TriggerAreaDisable
 23  CameraEnable
 24  CameraDisable
 25  EnemySetTargetNode
 26  EnemySetNextTargetNode

Trigger types:
  0  Equal
  1  NotEqual
  2  Less
  3  Greater
  4  LessEqual
  5  GreaterEqual
  6  FlagCount
  7  FlagTest
"""

import struct
import sys

EVENT_NAMES = {
    0:  "PickupGenerate",
    1:  "EnemyGenerate",
    2:  "OpenDoor",
    3:  "CloseDoor",
    4:  "LockDoor",
    5:  "UnlockDoor",
    6:  "StartBGOAnim",
    7:  "StopBGOAnim",
    8:  "StartTexAnim",
    9:  "StopTexAnim",
    10: "ModifyTrigVar",
    11: "TriggerMod",
    12: "(reserved12)",
    13: "WaterFill",
    14: "(reserved14)",
    15: "WaterDrain",
    16: "StartSpotFX",
    17: "StopSpotFX",
    18: "ShowMessages",
    19: "EnableLight",
    20: "DisableLight",
    21: "TriggerAreaEnable",
    22: "TriggerAreaDisable",
    23: "CameraEnable",
    24: "CameraDisable",
    25: "EnemySetTargetNode",
    26: "EnemySetNextTargetNode",
}

TRIGGER_TYPE_NAMES = {
    0: "Equal",
    1: "NotEqual",
    2: "Less",
    3: "Greater",
    4: "LessEqual",
    5: "GreaterEqual",
    6: "FlagCount",
    7: "FlagTest",
}

TRIGMOD_OP_NAMES = {
    0: "Set",
    1: "Reset",
    2: "Inc",
    3: "Dec",
    4: "Or",
    5: "And",
    6: "Xor",
    7: "Multiply",
    8: "Divide",
    9: "Random",
    10: "Setflag",
    11: "Clearflag",
}

MAXDATAPEREVENT = 64


def parse_trg(data):
    pos = 0

    def read_i32():
        nonlocal pos
        v = struct.unpack_from('<i', data, pos)[0]
        pos += 4
        return v

    def read_u32():
        nonlocal pos
        v = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        return v

    def read_u8():
        nonlocal pos
        v = data[pos]
        pos += 1
        return v

    def read_f32():
        nonlocal pos
        v = struct.unpack_from('<f', data, pos)[0]
        pos += 4
        return v

    # Skip MAGIC (4 bytes) + VERSION (4 bytes)
    magic = data[pos:pos+4]
    pos += 4
    version = read_u32()
    print(f"Magic: {magic}  Version: {version}")

    # Header: 5 counts
    num_trigvars    = read_i32()
    num_triggers    = read_i32()
    num_trigmods    = read_i32()
    num_events      = read_i32()
    num_conditions  = read_i32()

    print(f"Header: TrigVars={num_trigvars}  Triggers={num_triggers}  "
          f"TrigMods={num_trigmods}  Events={num_events}  Conditions={num_conditions}")
    print()

    # TrigVars
    trigvars = []
    for i in range(num_trigvars):
        name_bytes = data[pos:pos+32]
        pos += 32
        name = name_bytes.split(b'\x00')[0].decode('latin-1')
        init_state = read_i32()
        n_trigs = read_i32()
        trig_indices = [read_i32() for _ in range(n_trigs)]
        trigvars.append({
            'idx': i,
            'name': name,
            'init_state': init_state,
            'trigger_indices': trig_indices,
        })

    # Triggers
    triggers = []
    for i in range(num_triggers):
        ttype = read_u8()
        trigvar_idx = read_i32()
        active_state = read_i32()
        n_cond = read_i32()
        cond_indices = [read_i32() for _ in range(n_cond)]
        triggers.append({
            'idx': i,
            'type': ttype,
            'trigvar_idx': trigvar_idx,
            'active_state': active_state,
            'condition_indices': cond_indices,
        })

    # TrigMods
    trigmods = []
    for i in range(num_trigmods):
        op = read_i32()
        trigvar_idx = read_i32()
        val = read_i32()
        time = read_f32()
        next_idx = read_i32()
        trigmods.append({
            'idx': i,
            'op': op,
            'trigvar_idx': trigvar_idx,
            'val': val,
            'time_ticks': time * 60.0,
            'next_idx': next_idx,
        })

    # Events (byte-stream)
    events = []
    for i in range(num_events):
        etype = read_u8()
        edata = list(data[pos:pos+MAXDATAPEREVENT])
        pos += MAXDATAPEREVENT
        events.append({'idx': i, 'type': etype, 'data': edata})

    # Conditions (back to int32-aligned)
    # pos should already be aligned since events section is (1+64)*N bytes,
    # and header + trigvar/trigger/trigmod sections all end on int32 boundary.
    conditions = []
    for i in range(num_conditions):
        n_trigs = read_i32()
        trig_indices = [read_i32() for _ in range(n_trigs)]
        n_evts = read_i32()
        evt_indices = [read_i32() for _ in range(n_evts)]
        conditions.append({
            'idx': i,
            'trigger_indices': trig_indices,
            'event_indices': evt_indices,
        })

    return trigvars, triggers, trigmods, events, conditions


def describe_event(ev, trigvars):
    etype = ev['type']
    name = EVENT_NAMES.get(etype, f"Unknown({etype})")
    d = ev['data']
    # Most events use little-endian uint16 at d[0:2]
    val16 = struct.unpack_from('<H', bytes(d[:2]))[0]
    val32 = struct.unpack_from('<I', bytes(d[:4]))[0]
    extra = ""
    if etype in (2, 3, 4, 5, 6, 7):  # door / BGO anim
        extra = f"  bgo_idx={val16}"
    elif etype in (0, 1, 16, 17):  # pickup/enemy/spotfx
        extra = f"  idx={val16}"
    elif etype == 10:  # ModifyTrigVar
        tv_idx = struct.unpack_from('<H', bytes(d[:2]))[0]
        op = d[2]
        val = struct.unpack_from('<i', bytes(d[3:7]))[0]
        tv_name = trigvars[tv_idx]['name'] if tv_idx < len(trigvars) else f"?{tv_idx}"
        op_name = TRIGMOD_OP_NAMES.get(op, f"op{op}")
        extra = f"  trigvar={tv_name}({tv_idx}) op={op_name} val={val}"
    elif etype == 11:  # TriggerMod
        extra = f"  trigmod_idx={val16}"
    elif etype in (25, 26):  # EnemySetTargetNode
        extra = f"  enemy_idx={d[0]|d[1]<<8} node={d[2]|d[3]<<8}"
    return f"  Event[{ev['idx']}] type={etype}({name}){extra}  raw={bytes(d[:8]).hex()}"


def print_all(trigvars, triggers, trigmods, events, conditions):
    print("=== TRIGVARS ===")
    for v in trigvars:
        print(f"  [{v['idx']}] '{v['name']}'  initState={v['init_state']}  "
              f"triggers={v['trigger_indices']}")
    print()

    print("=== TRIGGERS ===")
    for t in triggers:
        tv = trigvars[t['trigvar_idx']] if t['trigvar_idx'] < len(trigvars) else None
        tv_name = tv['name'] if tv else '?'
        ttype_name = TRIGGER_TYPE_NAMES.get(t['type'], f"type{t['type']}")
        print(f"  [{t['idx']}] {tv_name}({t['trigvar_idx']}) "
              f"{ttype_name} {t['active_state']}  conds={t['condition_indices']}")
    print()

    print("=== TRIGMODS ===")
    for m in trigmods:
        tv = trigvars[m['trigvar_idx']] if m['trigvar_idx'] < len(trigvars) else None
        tv_name = tv['name'] if tv else '?'
        op_name = TRIGMOD_OP_NAMES.get(m['op'], f"op{m['op']}")
        nxt = m['next_idx'] if m['next_idx'] >= 0 else None
        print(f"  [{m['idx']}] {op_name} {tv_name}({m['trigvar_idx']}) "
              f"val={m['val']}  time={m['time_ticks']:.1f}ticks  next={nxt}")
    print()

    print("=== EVENTS ===")
    for ev in events:
        print(describe_event(ev, trigvars))
    print()

    print("=== CONDITIONS ===")
    for c in conditions:
        ev_descs = [describe_event(events[ei], trigvars) for ei in c['event_indices'] if ei < len(events)]
        print(f"  [{c['idx']}] triggers={c['trigger_indices']}  events={c['event_indices']}")
        for ed in ev_descs:
            print(f"    {ed.strip()}")
    print()


def find_switch_chains(trigvars, triggers, trigmods, events, conditions):
    """Find TrigVars that look like switches/levers and print their full chain."""
    switch_keywords = ['lever', 'switch', 'button', 'key', 'panel', 'console']
    print("=== SWITCH/LEVER CHAINS ===")
    for v in trigvars:
        name_lower = v['name'].lower()
        if any(kw in name_lower for kw in switch_keywords):
            print(f"\n--- TrigVar[{v['idx']}] '{v['name']}' initState={v['init_state']} ---")
            for ti in v['trigger_indices']:
                if ti >= len(triggers):
                    continue
                t = triggers[ti]
                ttype_name = TRIGGER_TYPE_NAMES.get(t['type'], f"type{t['type']}")
                print(f"  Trigger[{ti}]: when {v['name']} {ttype_name} {t['active_state']}")
                for ci in t['condition_indices']:
                    if ci >= len(conditions):
                        continue
                    c = conditions[ci]
                    print(f"    Condition[{ci}]: all_of triggers={c['trigger_indices']}")
                    for ei in c['event_indices']:
                        if ei >= len(events):
                            continue
                        print(f"     {describe_event(events[ei], trigvars)}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "/mnt/nas/Development/forsaken/Data/levels/defend2/defend2.trg"
    with open(path, 'rb') as f:
        data = f.read()
    print(f"Parsing {path}  ({len(data)} bytes)\n")
    trigvars, triggers, trigmods, events, conditions = parse_trg(data)
    print_all(trigvars, triggers, trigmods, events, conditions)
    find_switch_chains(trigvars, triggers, trigmods, events, conditions)


if __name__ == '__main__':
    main()
