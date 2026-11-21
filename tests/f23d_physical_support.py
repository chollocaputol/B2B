from f23d_sd_support import remove_sd
"""Restore accepted source beneath independently marked physical additions."""
def remove_physical(text):
    text=remove_sd(text)
    out=[];inside=False
    for line in text.splitlines(keepends=True):
        if line.strip() in ("// F23D physical begin","; F23D physical begin"):
            assert not inside;inside=True
        elif line.strip() in ("// F23D physical end","; F23D physical end"):
            assert inside;inside=False
        elif inside and line.startswith("// F23D physical restore: "):
            out.append(line[len("// F23D physical restore: "):])
        elif not inside:out.append(line)
    assert not inside
    return "".join(out)
