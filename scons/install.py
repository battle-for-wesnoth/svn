# vi: syntax=python:et:ts=4
from SCons.Script import *
import shutil, os
from subprocess import call, Popen, PIPE

def InstallFilteredHook(target, source, env):
    CopyFilter = env["copy_filter"]
    target = Flatten(target)
    source = Flatten(source)
    if(len(target) != len(source)):
        raise ValueError, "Number of targets doesn't match number of sources"

    def do_copy(target, source):
        if CopyFilter(source):
            if os.path.isfile(source):
                if (env["gui"] == "tiny") and (source.endswith("jpg") or source.endswith("png")):
                    image_info = Popen(["identify", "-verbose", source], stdout = PIPE).communicate()[0]
                    if "Alpha: " in image_info:
                        command = "convert -filter point -resize %s %s %s"
                    else:
                        command = "convert -resize %s %s %s"
                    for (large, small) in (("1024x768","320x240"),
                                            ("640x480","240x180"),
                                            ("205x205","80x80")):
                        if ("Geometry: " + large) in image_info:
                            command = command % (small, source, target)
                            break
                    else:
                            command = command % ("50%", source, target)
                    if env["verbose"]:
                        print command
                    call(Split(command))
                else:
                    # Just copy non-images, and images if tinygui is off
                    if env["verbose"]:
                        print "cp %s %s" % (source, target)
                    shutil.copy2(source, target)
            else:
                if not os.path.exists(target):
                    if env["verbose"]:
                        print "Make directory", target
                    os.makedirs(target)
                for file in os.listdir(source):
                    do_copy(os.path.join(target, file), os.path.join(source, file))

    for (target_dir, source_dir) in zip(target, source):
        target_path = str(target_dir)
        source_path = str(source_dir)
        if not os.path.exists(target_path):
            os.makedirs(target_path)
        for d in (target_path, source_path):
            if not os.path.isdir(d):
                raise ValueError, "%s is not a directory" % d
        do_copy(target_path, source_path)

from SCons.Action import ActionFactory
from shutil import copy2
def hard_link(dest, src, symlink = False):
    try:
        if symlink:
            os.symlink(src, dest)
        else:
            os.link(src, dest)
    except OSError, e:
        if e.errno == 18:
            hard_link(dest, src, True)
        else:
            os.remove(dest)
            os.link(src, dest)
    except AttributeError:
        copy2(src, dest)

HardLink = ActionFactory(hard_link,
                         lambda dest, src: 'Hardlinking %s to %s' % (src, dest))

def InstallBinary(env, source):
    if not source:
        return source

    binary = source[0].name
    installdir = env.subst(os.path.join(env["destdir"], env["bindir"].lstrip("/")))
    env.Alias("install-" + binary, 
        env.InstallAs(os.path.join(installdir, binary + env["program_suffix"]), source)
    )

def InstallData(env, datadir, component, source, subdir = ""):
    installdir = Dir(env.subst(os.path.join(env["destdir"], env[datadir].lstrip("/"), subdir)))
    sources = map(Entry, Flatten([source]))
    dirs = []
    for source in sources:
        if source.exists():
            if source.isfile():
                env.Alias("install-" + component, env.Install(installdir, source))
            else:
                dirs.append(Dir(source))
    if dirs:
        install = map(lambda x : env.InstallFiltered(os.path.join(installdir.path, x.name), x.path), dirs)
        AlwaysBuild(install)
        env.Alias("install-" + component, install)

def generate(env):
    #env.AddMethod(InstallWithSuffix)
    from SCons.Script.SConscript import SConsEnvironment
    SConsEnvironment.InstallBinary = InstallBinary
    SConsEnvironment.InstallData = InstallData

    env.Append(BUILDERS={'InstallFiltered':Builder(action=InstallFilteredHook, target_factory=Dir, source_factory=Dir)})

def exists():
    return True
