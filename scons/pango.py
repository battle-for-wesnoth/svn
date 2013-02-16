# vi: syntax=python:et:ts=4
import os, sys
from os.path import join
from os import environ
from SCons.Util import AppendPath

def CheckPango(context, backend, require_version = None):
    context.Message("Checking for Pango with " + backend + " backend... ")
    env = context.env
    gtkdir = env.get("gtkdir", os.environ.get("GTK_BASEPATH"))
    if gtkdir:
        environ["PATH"] = AppendPath(environ["PATH"], join(gtkdir, "bin"))
        environ["PKG_CONFIG_PATH"] = AppendPath(environ.get("PKG_CONFIG_PATH", ""), join(gtkdir, "lib/pkgconfig"))
        if sys.platform != "win32":
            env["PKGCONFIG_FLAGS"] = "--define-variable=prefix=" + gtkdir

    try:
        env["ENV"]["PKG_CONFIG_PATH"] = environ.get("PKG_CONFIG_PATH")
        version_arg = ""
        if require_version:
            version_arg = "--atleast-version=" + require_version
        env.ParseConfig("pkg-config --libs --cflags %s $PKGCONFIG_FLAGS pango" % version_arg + backend)
        context.Result("yes")
        return True
    except OSError:
        context.Result("no")
        return False

config_checks = { "CheckPango" : CheckPango }
