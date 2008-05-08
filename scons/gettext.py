# vi: syntax=python:et:ts=4
from os.path import join
from SCons.Builder import Builder
from SCons.Script  import *

def exists():
    return True

def generate(env):
    env.AppendENVPath("PATH", join(env["gettextdir"], "bin"))
    env["MSGFMT"] = WhereIs("msgfmt")
    msgfmt = Builder(
        action = "$MSGFMT -c --statistics -o $TARGET $SOURCE",
        src_suffix = ".po",
        suffix = ".mo",
        single_source = True
        )
    env["BUILDERS"]["Msgfmt"] = msgfmt

    env["MSGMERGE"] = WhereIs("msgmerge")
    msgmerge = Builder(
        action = "$MSGMERGE $TARGET $SOURCE -o $TARGET",
        src_suffix = ".pot",
        suffix = ".po",
        single_source = True
        )
    env["BUILDERS"]["MsgMerge"] = msgmerge
