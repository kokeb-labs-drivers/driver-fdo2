"""One command and its reply, with the time each part may take.

Reads: A transaction is five steps, left to right. The driver drops what is in the receive buffer, writes the command
and its carriage return, then waits for the module: the module echoes the command, with its values, and ends the
reply with one carriage return. The wait is the characters' airtime at the current baud rate, plus the module's
processing allowance for the command (command_ms, measurement_ms, calibration_ms, flash_ms or logo_ms of the Config),
plus margin_ms. The numbers shown are the Config defaults, read from inc/fdo2_types.hpp. A reply that does not end in
time is a Timeout: the driver drains the line, and sends an idempotent read again when retries allow.
"""

from _source import TYPES, config_default

from figkit import Fig


def data():
    keys = ["command_ms", "measurement_ms", "calibration_ms", "flash_ms", "logo_ms", "margin_ms"]
    return {"ms": {k: config_default(k) for k in keys}, "sources": [f"{TYPES}:Config"]}


def build(d):
    ms = d["ms"]
    f = Fig(
        "transaction-timing",
        "One command and its reply",
        alt=("Five steps of a transaction: flush the receive buffer, write the command, wait for the module, "
             "read the echo and values, end at the carriage return; the allowance of each kind of command is "
             "listed under the wait."),
        width=1200,
        kicker="Timing",
        subtitle="The wait is airtime plus the module's allowance plus the margin; the defaults are shown.",
        legend=[("brand", "the driver"), ("teal", "the module"), ("~solid", "then")],
    )
    W = f.body_w
    gap = 36
    cw = (W - 4 * gap) / 5
    steps = [
        ("1. Flush", ["FlushInput()", "no late byte survives"], "brand"),
        ("2. Write", ["#CMD args CR", "one Write()"], "brand"),
        ("3. Wait", ["module processes", "Read() with timeouts"], "teal"),
        ("4. Reply", ["echo, values, CRC", "up to 799 characters"], "teal"),
        ("5. CR", ["one CR ends it", "Ok, or Timeout and drain"], "brand"),
    ]
    cards = []
    for i, (title, lines, tone) in enumerate(steps):
        cards.append(f.card(i * (cw + gap), 0, cw, title, lines, tone=tone, accent=True))
    for a, b in zip(cards, cards[1:]):
        f.connect(a, b, "right", "left")
    f.note(0, cards[0].b + 40, W,
           f"Allowance in step 3 by command: {ms['command_ms']} ms for #VERS #IDNR #RDUM #BAUD; "
           f"{ms['measurement_ms']} ms for #MOXY and #MRAW; {ms['calibration_ms']} ms for #CALO and #CAHI; "
           f"{ms['flash_ms']} ms for #CRCE #WRUM #BCST #SETM; {ms['logo_ms']} ms for #LOGO; plus "
           f"{ms['margin_ms']} ms of margin and the airtime of the command and of the longest reply.",
           tone="teal", title="Allowances")
    return f
