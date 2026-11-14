# GUI + run logging

## Play

    pip install pygame
    python3 gui.py                    # random real seed
    python3 gui.py --seed QJSKRTWR

Click cards/jokers/consumables to select; buttons appear per phase and grey
out when the engine says the action is illegal (same legal mask the AI sees).
Keys: 1-9,0,-,= toggle cards · Enter play · D discard · C clear · S select
blind · K skip blind · Space cash-out/leave-shop/skip-pack · R reroll ·
arrows shift the selected card · Esc quit. On game over: N new run, S same
seed.

## Run logs (the training part)

Every run auto-writes `runs/run_<stamp>_<seed>.jsonl` (crash-safe: every line
is flushed). The engine is deterministic in (seed, actions), so the log
doesn't need to store observations — `runlog.py` re-simulates them:

    python3 runlog.py summarize runs/           # what have I got
    python3 runlog.py verify runs/x.jsonl       # re-simulate, check legality
    python3 runlog.py export runs/x.jsonl x.npz # -> obs(N,222) legal(N,58) action(N,)

or in your training script:

    from runlog import replay
    for obs, legal_mask, action in replay("runs/x.jsonl"):
        ...   # obs/mask are the state BEFORE action — behaviour cloning ready

`--log-obs` embeds the 222-float obs in each step line if you'd rather not
re-simulate (files get ~20x bigger; replay stays the source of truth).

`verify` failing with "illegal on replay" means the log was recorded on a
different engine build — keep engine changes and logged datasets together.

## Clearing runs

Logs record a fingerprint of the engine `.so` they were made with, so a
dataset from an older build is visibly `STALE` in `summarize` (and will
usually fail `verify`).

    python3 runlog.py clear runs/                 # delete everything
    python3 runlog.py clear runs/ --stale-only    # only other-build runs
    python3 runlog.py clear runs/ --keep-complete # drop abandoned runs only
    python3 runlog.py clear runs/ --dry-run       # preview, delete nothing

`rm -rf runs/` works too — the directory is recreated on the next run.
