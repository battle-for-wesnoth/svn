#!/usr/bin/env python
# vim: tabstop=4: shiftwidth=4: expandtab: softtabstop=4: autoindent:
# $Id$ 

"""
   Copyright (C) 2007 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
"""   

import operator, os, re

if __name__ == "__main__":
    # contains all output generated:
    # - key filename 
    # - value node list
    #
    # every node is a list with 2 items 
    # - first the sorting order
    # - second the actual data
    file_map = {}

    # default directory to dump the output in with trailing /.
    output_directory = "/tmp/"

    # default directory to find the source files in, no trailing /.
    src_directory = "../../src/gui"

    def reindent( data):
        """Converts the raw input to an easier to use format.
        
        Lines starting with 8 spaces are concatenated with the previous line.
        The start of line ' *' are removed and if another space exists it's also
        removed."""

        # concatenate 
        data = re.sub(r'\n \*(?: ){8,}', " ", data)

        # strip 
        data = re.sub(" \*(?: |)", "", data)

        return data

    def get_value(data, key):
        """Extracts data from a key value pair, a key must start at the start of a line."""

        key1 = "^" + key
        sep = "(?: )*=(?: )"
        value = "(.*)$"
        res = re.compile("^" + key + " *= *(.*)$", re.M).search(data)

        if(res != None):
            res = res.group(1)

        return res

    def process_header(data):
        """Processes the header."""

        page = get_value(data, "@page")
        order = get_value(data, "@order")
        if(order == None):
            order = 10000

        return [page, order]

    def create_config_table(data):
        """Creates a table for data in a config table.

        A config table is a table with info about WML configuration key value pairs.
        """
    
        # matches a line like
        # x1 (f_unsigned = 0)             The x coordinate of the startpoint.
        # x1 (f_unsigned)                 The x coordinate of the startpoint.
        variable = "(?:[a-z]|[A-Z])(?:[a-z]|[A-Z]|[0-9]|_)*"
        regex = re.compile(" *(" + variable +  ") \((" + variable + ") *(?:(?:= *(.*))|)\) +(.*)\n")
        res = regex.findall(data)

        # empty table
        if(len(res) == 0):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!key\n!type\n!default\n!description\n"
        for i in range(len(res)):
            result += "|-\n"
            result += "|" + res[i][0] + "\n"
            result += "|[[GUIVariable#" + res[i][1] + "|" + res[i][1] + "]]\n"
            if(res[i][2] == ""):
                result += "|mandatory\n"
            else:
                result += "|" + res[i][2] + "\n"
            result += "|" + res[i][3] + "\n"
        result += "|}"

        return result

    def create_formula_table(data):
        """Creates a table for data in a formula table.

        A formula table is a table with info about which function parameters 
        are available for processing in WML formulas.
        """

        #matches a line like
        #width unsigned                  The width of the canvas.
        variable = "(?:[a-z]|[A-Z])(?:[a-z]|[A-Z]|[0-9]|_)*"
        regex = re.compile(" *(" + variable +  ") (" + variable + ") +(.*)\n")
        res = regex.findall(data)

        # empty table
        if(len(res) == 0):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Variable\n!type\n!description\n"
        for i in range(len(res)):
            result += "|-\n"
            result += "|" + res[i][0] + "\n"
            result += "|" + res[i][1] + "\n"
            result += "|" + res[i][2] + "\n"
        result += "|}"

        return result

    def create_variable_types_table(data):
        """Creates a table for the variable types."""

        #matches a line like
        # int                             Signed number (whole numbers).
        variable = "(?:[a-z]|[A-Z])(?:[a-z]|[A-Z]|[0-9]|_)*"
        regex = re.compile(" *(" + variable +  ") +(.*)\n")
        res = regex.findall(data)

        # empty table
        if(len(res) == 0):
            return "Empty table."

        result = '{| border="1"'
        result += "\n!Variable\n!description\n"
        for i in range(len(res)):
            result += "|-\n"
            result += "|<span id=\"" + res[i][0] + "\">" + res[i][0] + "</span>\n"
            result += "|" + res[i][1] + "\n"
        result += "|}"

        return result
        

    def create_table(table) :
        """Wrapper for creating tables."""

        type = table.group(1)
        if(type == "config"):
            return create_config_table(table.group(2))
        elif(type == "formula"):
            return create_formula_table(table.group(2))
        elif(type == "variable_types"):
            return create_variable_types_table(table.group(2))
        else:
            return "unknown table definition '" + type + "'."

    def process_body(data):
        """Process the body.
        
        The body needs to be stripped of known markup values.
        """

        table_regex = re.compile("^@start_table *= *(.*?)\n(.*?)\n@end_table.*?$", re.M | re.S)
        data = table_regex.sub(lambda match: create_table(match), data)
        return data

    def process(data):
        """Processes a wiki block."""

        res = re.compile("(.*?)\n\n(.*)", re.S).findall(data);
        if(res != None and len(res) != 0):
            header = process_header(res[0][0])
            body = process_body(res[0][1])
        else:
            print "Invalid wiki block, discarded."
            return

        if(header[0] == None):
            print "No page defined, dropped."
            return

        if(file_map.has_key(header[0]) == False):
            file_map[header[0]] = []

        file_map[header[0]].append([header[1], body])

    def create_output():
        """Generates the output"""

        for file, data_list in file_map.iteritems():
            data_list.sort(key=operator.itemgetter(0))
            fd = open(output_directory + file, "w")
            for i in range(len(data_list)):
                fd.write(data_list[i][1])
            fd.close()
    
    def process_file(name):
        """Processes all wiki blocks (if any) of a file."""

        file = open(name, "r")
        data = file.read()
        file.close()

        regex = re.compile("(/\*WIKI($.*?)^ \*/)", re.M | re.S)
        res = regex.findall(data)
        if(res != None):
            for i in range(len(res)):
                section = reindent(res[i][1])
                wiki_info = process(section)
        else:
            print "No Wiki block found."

    def process_directory(dir):
        """Processes a directory.

        Recurses into the sub directory, ignoring hidden directories.
        Processes every .cpp file.
        """

        items = os.listdir(dir)

        for item in items:

            # Ignore .svn dirs.
            if(item.startswith(".")):
                continue

            if(os.path.isdir(dir + "/" + item)):
                process_directory(dir + "/" + item)
            elif(item.endswith(".cpp")):
                process_file(dir + "/" + item)



    ##### ##### ##### MAIN ##### ##### #####


    process_directory(src_directory)

    create_output()

