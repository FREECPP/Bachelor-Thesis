Import("env")
import subprocess
from datetime import datetime

try:
    count_str = subprocess.check_output(
        ["git", "rev-list", "--count", "HEAD"],
        cwd=env["PROJECT_DIR"]
    ).decode().strip()
    count = int(count_str)
except Exception:
    count = 0

major = count // 9
minor = count % 9
date_short = datetime.now().strftime("%d.%m.%Y")
version_str = "v0.{}{}pre/{}".format(major, minor, date_short)

env.Append(CPPDEFINES=[("SW_VERSION", env.StringifyMacro(version_str))])
