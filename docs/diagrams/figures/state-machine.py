"""The driver's states and what moves it between them.

Reads: Each box is a State of fdo2::FDO2, read from the enumeration in inc/fdo2_types.hpp; each arrow is a call or an
event that moves it. Start-up goes through PoweringUp to Idle. An operation (Start* or a blocking call) puts the driver
in Waiting; a good reply returns it to Idle. A timeout, a CRC error or a refused line sends it through Draining, where
input is discarded until the line is quiet, so a late reply is never read as the next answer. A flash-writing command
or a baud change that ended without a confirmed echo leaves StateUnknown, which only Resync() or a power cycle ends.
While the module streams, Idle is reported as Streaming. The transitions are fixed in this figure and checked
against the names used in src/fdo2.ipp.
"""

from _source import IMPL, TYPES, enumerators, need, read

from figkit import Fig

EDGES = [
    ("Uninitialized", "PoweringUp", "BeginInitialize()"),
    ("PoweringUp", "Idle", "#VERS, #IDNR ok"),
    ("Idle", "Waiting", "Start*() / blocking call"),
    ("Waiting", "Idle", "reply accepted"),
    ("Waiting", "Draining", "Timeout, Crc, InvalidReply"),
    ("Draining", "Idle", "line quiet"),
    ("Waiting", "StateUnknown", "flash write or #BAUD, no echo"),
    ("StateUnknown", "Idle", "Resync()"),
]


def data():
    states = enumerators(TYPES, "State")
    impl = read(IMPL)
    for s in states:
        need(rf"State::{s}\b", impl, f"State::{s} in the implementation")
    for a, b, _ in EDGES:
        if a not in states or b not in states:
            raise SystemExit(f"figure source drift: {a} or {b} is not a State")
    return {"states": states, "edges": EDGES, "sources": [f"{TYPES}:State", f"{IMPL}:State transitions"]}


def build(d):
    f = Fig(
        "state-machine",
        "The driver's states",
        alt=("State diagram of fdo2::FDO2: Uninitialized, PoweringUp, Idle, Waiting, Streaming, Draining and "
             "StateUnknown, with the calls and events that move between them."),
        width=1200,
        kicker="Non-blocking use",
        subtitle="Poll() moves Waiting, Draining and PoweringUp forward; every other change is a call.",
        legend=[("brand", "normal path"), ("amber", "needs recovery"), ("~solid", "moves to")],
    )
    W = f.body_w
    gap = 190
    cw = (W - 2 * gap) / 3
    ch = 90
    row1, row2, row3 = 0, 190, 380
    xs = [i * (cw + gap) for i in range(3)]
    tones = {"Uninitialized": "slate", "PoweringUp": "brand", "Idle": "teal", "Waiting": "brand",
             "Streaming": "teal", "Draining": "amber", "StateUnknown": "rose"}
    notes = {
        "Uninitialized": "nothing started, or Deinitialize()",
        "PoweringUp": "waits power_up_ms, then #VERS and #IDNR",
        "Idle": "ready for a call",
        "Waiting": "one command in flight, Poll() reads the reply",
        "Streaming": "Idle while the module sends #MRAW lines by itself",
        "Draining": "discards input after a bad reply",
        "StateUnknown": "a flash write or #BAUD lost its echo",
    }
    pos = {"Uninitialized": (xs[0], row1), "PoweringUp": (xs[1], row1), "Idle": (xs[2], row1),
           "Waiting": (xs[2], row2), "Draining": (xs[1], row2), "StateUnknown": (xs[2], row3),
           "Streaming": (xs[0], row2)}
    cards = {}
    for s in d["states"]:
        x, y = pos[s]
        cards[s] = f.card(x, y, cw, s, [notes[s]], tone=tones[s], accent=True)
    f.connect(cards["Uninitialized"], cards["PoweringUp"], "right", "left", label="BeginInitialize()")
    f.connect(cards["PoweringUp"], cards["Idle"], "right", "left", label="#VERS, #IDNR ok")
    f.connect(cards["Idle"], cards["Waiting"], "bottom", "top", at=0.3, bt=0.3, label="Start*()")
    f.connect(cards["Waiting"], cards["Idle"], "top", "bottom", at=0.7, bt=0.7, label="reply")
    f.connect(cards["Waiting"], cards["Draining"], "left", "right", label="Timeout, Crc")
    f.connect(cards["Waiting"], cards["StateUnknown"], "bottom", "top", label="no echo")
    f.note(0, row3, xs[2] - 40, "Resync() leaves StateUnknown: it reads #VERS at the new, the old and the default baud rate.",
           tone="amber", title="Recovery")
    return f
