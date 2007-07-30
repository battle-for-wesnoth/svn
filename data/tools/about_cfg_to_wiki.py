#!/usr/bin/env python
# encoding: utf8
#

#
# This must be run from the directory which has Wesnoth's "data" folder in it.
#

import sys, re, glob

import wesnoth.wmldata as wmldata

def output(text):
    sys.stdout.write(text.encode("utf8"))

if __name__ == "__main__":
    import optparse, subprocess
    try: import psyco
    except ImportError: pass
    else: psyco.full()

    optionparser = optparse.OptionParser()
    options, args = optionparser.parse_args()

    files = ["data/core/about.cfg"]
    files.extend(glob.glob("data/campaigns/*/_main.cfg"))

    # Parse WML.
    class Section: pass
    class Entry: pass
    chapters = []
    for arg in files:
        sections = []
        wml = wmldata.read_file(arg)
        if not wml.get_first("about"):
            wml = wml.get_first("campaign")
        if not wml.get_first("about"):
            sys.stderr.write("No about section found in %s\n" % arg)
        for about in wml.get_all("about"):
            section = Section()
            section.title = about.get_text_val("title")
            section.lines = []
            for entry in about.get_all("entry"):
                name = entry.get_text_val("name")
                if name == "*": continue
                comment = entry.get_text_val("comment", "")
                wikiuser = entry.get_text_val("wikiuser", "")
                email = entry.get_text_val("email", "")
                section.lines.append((name, comment, wikiuser, email))
            if section.title: sections.append(section)
        chapters.append((arg, sections))

    # Output.
    output("""
{| style="float:right"
|
__TOC__
|}
In July 2003, '''David White''' released the first version of Wesnoth. Since
then, many people have joined the project, contributing in very different ways.

To make any changes to this list, please modify about.cfg in SVN or ask any
developer to do it for you.

""")
    for path, sections in chapters:
        if path == "data/core/about.cfg":
            output("== Contributors ==\n")
        else:
            slash1 = path.rfind("/")
            slash2 = path.rfind("/", 0, slash1)
            beautified = path[slash2 + 1:slash1]
            beautified = beautified.replace("_", " ")
            beautified = beautified[0].upper() + beautified[1:]
            output("== " + beautified + " ==\n")
        for section in sections:
            output("=== %s ===\n" % section.title)
            for name, comment, wikiuser, email in section.lines:
                if comment: comment = " - " + comment
                if wikiuser:
                    if "(" in name:
                        name = re.sub(r"\((.*)\)", "([[User:%s|\\1]])" % wikiuser, name)
                    else:
                        name = "[[User:%s|%s]]" % (wikiuser, name)
                if email:
                    if "(" in name:
                        name, nick = name.split("(", 1)
                        name = name.strip()
                        name = "[mailto:" + email + " " + name + "]"
                        name += " (" + nick
                    else:
                        name = "[mailto:" + email + " " + name + "]"
                output("* %s%s\n" % (name, comment))

    output("""{{Home}}""")
