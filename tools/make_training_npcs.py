"""One-off: build ui/training_npcs.json (the game's CPU units = NPCs for the Custom Training unit list).

    py -3.14 tools/make_training_npcs.py <game folder> ui/training_npcs.json

Reads the game files with the EXVS Mod Manager source (sibling folder ../EXVSModManager, or $EXVS_MODMANAGER_DIR):
the CPU unit DB 0xC5E73FE8 (names, VS picture) and the moveset of each unit (bosses.listing(npc=True)).
Output: a JSON array [[id, name, 0, "npc", thumb, may_not_work]], thumb = a 171x78 webp data URL
made from the VS picture (the unit's upper body, like the unit select tiles), may_not_work = 1 when its
moveset leaves out some player movements (it may not answer the stick). The file is a resource of the exe
(src/app.rc), sent to the page by the trainingNpcs call: inlined in the page it made the HTML too big for
WebView2's NavigateToString (blank window).
"""
import base64
import io
import os
import sys

game, out = os.path.abspath(sys.argv[1]), sys.argv[2]
os.environ["EXVS_GAME"] = game
mm_dir = os.environ.get("EXVS_MODMANAGER_DIR") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "EXVSModManager")
sys.path.insert(0, os.path.join(mm_dir, "src", "app"))
import paths  # noqa: E402

paths.setup()
import json  # noqa: E402

import bosses  # noqa: E402
import clones  # noqa: E402
import mod_manager  # noqa: E402
import textures  # noqa: E402
from PIL import Image  # noqa: E402

ctx = mod_manager.clone_context()
_, tb = bosses.boss_table(ctx, True)
rows = []
for b in sorted(bosses.listing(ctx, npc=True), key=lambda b: b["name"].lower()):
    if not b["complete"]:
        continue
    vs = textures.decode(ctx.read(clones.fname(tb.get(tb.row(b["id"]), clones.ART["ms_vs_r"]))).subfile(0)).convert("RGBA")
    unit = vs.crop(vs.getbbox() or (0, 0, vs.width, vs.height))
    w, h = 171, 78                                   # band of the upper body, as bosses.make_art does for ms_ms_s
    band = min(unit.height, max(1, round(unit.width * h / w)))
    y = min(round(unit.height * 0.1), unit.height - band)
    im = bosses._fit(unit.crop((0, y, unit.width, y + band)), w, h)
    bg = Image.new("RGBA", im.size, (11, 16, 28, 255))
    bg.alpha_composite(im)
    buf = io.BytesIO()
    bg.convert("RGB").save(buf, "WEBP", quality=72, method=6)
    rows.append([b["id"], b["name"], 0, "npc", "data:image/webp;base64," + base64.b64encode(buf.getvalue()).decode(),
                 1 if b["bugged"] else 0])
with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write(json.dumps(rows, ensure_ascii=False, separators=(",", ":")))
print(len(rows), "NPCs,", os.path.getsize(out), "bytes")
