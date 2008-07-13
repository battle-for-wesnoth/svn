# vi: syntax=python:et:ts=4
import os
from os.path import join
from os import environ
from SCons.Util import AppendPath

def CheckPango(context, backend):
    context.Message("Checking for Pango with " + backend + " backend... ")
    env = context.env
    gtkdir = env.get("gtkdir", os.environ.get("GTK_BASEPATH"))
    if gtkdir:
        environ["PATH"] = AppendPath(environ["PATH"], join(gtkdir, "bin"))
        environ["PKG_CONFIG_PATH"] = AppendPath(environ.get("PKG_CONFIG_PATH", ""), join(gtkdir, "lib/pkgconfig"))

    try:
        env.ParseConfig("pkg-config --libs --cflags pango" + backend)
        context.Result("yes")
        return True
    except OSError:
        context.Result("no")
        return False

config_checks = { "CheckPango" : CheckPango }
