#!/usr/bin/env python
# -*- coding: utf-8 -*-

# pofix - perform string fixups on incoming .po files.
#
# The purpose of this script is to save translators from having to
# apply various string fixes needed before stable release by hand.  It is
# intended to be run on each incoming .po file as the Lord of
# Translations receives it.  However, translators may run it on their
# own .po files to be sure, as a second application will harmlessly do
# nothing.
#
# To use this script, give it one or more paths to .po files as
# command-line arguments.  Each file will be tweaked as needed.
# It should work on Windows and MacOS X as well as Linux, provided
# you have Python installed.
#
# This script will emit a report line for each file it modifies,
# and save a backup copy of the original with extension "-bak".
#
# This script will tell you when it is obsolete.  Run it against all .po
# files in the main Wesnoth tree; when it says none are older than this script,
# it can be discarded (assunming that it has in fact been used to transform
# all incoming .po files in the meantime).
#
# Example usage:
# utils/pofix.py po/wesnoth*/*.po*
# find data/campaigns/ -name '*.cfg' -print0 | xargs -0 utils/pofix.py
#
# To make use of >1 CPU core, you have to rely on xargs. In this sample 10 files
# are handed over to 4 instances of pofix.py:
# ls po/wesnoth*/*.po* | xargs -P 4 -n 10 ./utils/pofix.py
#
#
# Please do make sure to add a comment before any new blocks of conversions
# that states when it was added (current version number is enough) so that
# the file can be cleaned up more easily every now and then.
# Example:
# # conversion added in 1.9.5+svn
# ("foo addwd bar", "foo added bar"),
# # conversion added in 1.9.8+svn
# ("fooba foo", "foobar foo"),

stringfixes = {

"1.8-announcement" : (
# conversion added shortly before 1.8.0, might be relevant for the 1.10.0 announcement
("WML events an AI components", "WML events and AI components"),
("1.7.3", "1.7.13"),
("/tags/1.8/", "/tags/1.8.0/"),
),

"1.10-announcement" : (
("roleplaying", "role-playing"),
),

}

# Speak, if all argument files are newer than this timestamp
# Try to use UTC here
# date --utc "+%s  # %c"
timecheck = 1283156523  # Mo 30 Aug 2010 08:22:03 UTC

import os, sys, time, stat, re
try:
    from multiprocessing import Pool, cpu_count
    def parallel_map(*args, **kw):
        pool = Pool(cpu_count())
        return pool.map(*args, **kw)
except ImportError:
    print "Failed to import 'multiprocessing' module. Multiple cpu cores won't be utilized"
    parallel_map = map

def process_file(path):
    before = open(path, "r").read()
    decommented = re.sub("#.*", "", before)
    lines = before.split('\n')
    for (domain, fixes) in stringfixes.items():
        # In case of screwed-up pairs that are hard to find, uncomment the following:
        #for fix in fixes:
        #    if len(fix) != 2:
        #        print fix
        for (old, new) in fixes:
            if old is new:
                #complain loudly
                print "pofix: old string\n\t\"%s\"\n equals new string\n\t\"%s\"\nexiting." % (old, new)
                sys.exit(1)
            #this check is problematic and the last clause is added to prevent false
            #positives in case that new is a substring of old, though this can also
            #lead to "real" probs not found, the real check would be "does replacing
            #old with new lead to duplicate msgids? (including old ones marked with #~)"
            #which is not easily done in the current design...
            elif new in decommented and old in decommented and not new in old:
                print "pofix: %s already includes the new string\n\t\"%s\"\nbut also the old\n\t\"%s\"\nthis needs handfixing for now since it likely creates duplicate msgids." % (path, new, old)
            else:
                for (i, line) in enumerate(lines):
                    if line and line[0] != '#':
                        lines[i] = lines[i].replace(old, new)
    after = '\n'.join(lines)
    if after != before:
        print "pofix: %s modified" % path
        # Save a backup
        os.rename(path, path + "-bak")
        # Write out transformed version
        ofp = open(path, "w")
        ofp.write(after)
        ofp.close()
        return 1
    else:
        return 0

if __name__ == '__main__':
    newer = 0
    modified = 0
    pocount = 0
    files = []
    for path in sys.argv[1:]:
        if not path.endswith(".po") and not path.endswith(".pot") and not path.endswith(".cfg"):
            continue
        pocount += 1
        # Notice how many files are newer than the time check
        statinfo = os.stat(path)
        if statinfo.st_mtime > timecheck:
            newer += 1
	files.append(path)
    modified = sum(parallel_map(process_file, files))
    print "pofix: %d files processed, %d files modified, %d files newer" \
          % (pocount, modified, newer)
    if pocount > 1 and newer == pocount:
        print "pofix: script may be obsolete"
