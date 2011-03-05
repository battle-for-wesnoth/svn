#!/usr/bin/python
#-*- coding:utf-8 -*-

"""
A script to create the "Terrain Table" on the TerrainCodesWML wiki page.
Run this and splice the output into the wiki whenever you add a new
terrain type to mainline.
"""

from __future__ import with_statement   # For python < 2.6
import os
import sys
import re
try:
    import argparse
except ImportError:
    print 'Please install argparse by running "easy_install argparse"'
    sys.exit(1)


def parse_terrain(data):
    """
    Parses the terrains. Input looks like this:

    [terrain_type]
    symbol_image=water/ocean-grey-tile
    id=deep_water_gray
    editor_name= _ "Gray Deep Water"
    string=Wog
    aliasof=Wo
    submerge=0.5
    editor_group=water
    [/terrain_type]

    Ouput is a text in wiki format.
    """

    # Remove all comments.
    data = "\n".join([i for i in data.split("\n") if not i.startswith("#")])
    terrains = re.compile("\[terrain_type\](.*?)\[\/terrain_type\]", re.DOTALL).findall(data)

    data = """{| border="1"
!name
!string
!alias of
!editor group
"""

    for i in terrains:
        # Strip unneeded things.
        i = i[5:]
        i = i.split("\n    ")
        # Don't parse special files that are hacks. They shouldn't be used
        # directly. (They're only there to make aliasing work.)
        if i[0].startswith(" "):
            continue
        # Create a dictionnary of key and values
        content = dict([v.split("=") for v in i])
        # Hidden things shouldn't be displayed
        if 'hidden' in content:
            continue
        data += """|-
| %s
| %s
| %s
| %s
""" % (
content['name'][4:-1] if 'name' in content else content['editor_name'][4:-1],
content['string'].lstrip(" # wmllint: ignore"),
content['aliasof'] if 'aliasof' in content else "",
content['editor_group'])

    data += "|}"
    return data

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='terrain2wiki is a tool to\
convert the terrain codes located in terrain.cfg to wiki formatted text.')
    parser.add_argument('-f', '--file', default='data/core/terrain.cfg',
dest='path', help="The location of the terrain.ctg file.")
    parser.add_argument('-o', '--output', default='output.tmp',
dest='output_path', help="The location of the ouput file.")
    args = parser.parse_args()

    path = args.path
    output_path = args.output_path

    if not os.path.exists(path) and not path.endswith('.cfg'):
        print "Invalid path: '%s' does not exist" % path
        sys.exit(1)

    with open(path, "r") as input_file:
        data = input_file.read()

    data = parse_terrain(data)

    with open(output_path, "w") as output:
        output.write(data)
