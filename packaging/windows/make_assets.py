# Puts the Windows icon where the setup and the package expect it. The icon itself is made, with the rest of
# the logo files, by packaging/brand/make_icons.py and kept in packaging/windows/everplay.ico.
import shutil
from pathlib import Path

here = Path(__file__).resolve().parent
out = here / "assets"
out.mkdir(exist_ok=True)
shutil.copyfile(here / "everplay.ico", out / "everplay.ico")
print("Done:", out)
