# encoding: utf8
import time, os, glob, sys

def output(path, url, data):
    try: os.mkdir(path)
    except OSError: pass

    f = open(path + "/index.html", "w")
    def w(x):
        f.write(x + "\n")
    w("""\
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<link rel=stylesheet href="style.css" type="text/css">
<script type="text/javascript" src="jquery.js"></script>
<script type="text/javascript" src="tablesorter.js"></script>
<script type="text/javascript">
$(document).ready(function() 
{ 
    $("#campaigns").tablesorter(
    {
        headers: { 1: { sorter: false} }
    }
    ); 
} 
); 
</script>
</head>
<body>""")

    w("""\
<div class="header">
<a href="http://www.wesnoth.org">
<img src="http://www.wesnoth.org/mw/skins/glamdrol/wesnoth-logo.jpg" alt="Wesnoth logo"/>
</a>
</div>
<div class="topnav">
<a href="index.html">Wesnoth Addons</a>
</div>
<div class="main">
<p>To install an add-on please go to the title screen of Battle for Wesnoth. Select "Add-ons" from the menu and click "OK" to connect to add-ons.wesnoth.org.
Select the add-on you want to install from the list and click "OK". The download will commence immediately. Wesnoth will then automatically install and load the add-on so you can use it.</p>
<p>Note: Hover over the icons to see the description of the add-on.</p>
""")

    am_dir = os.path.dirname(__file__) + "/"
    for name in ["style.css", "jquery.js", "tablesorter.js",
        "asc.gif", "bg.gif", "desc.gif"]:
        os.system("cp -u " + am_dir + name + " " + path)

    campaigns = data.get_or_create_sub("campaigns")
    w("<table class=\"tablesorter\" id=\"campaigns\">")
    w("<thead>")
    w("<tr>")
    for header in ["Type", "Icon", "Addon", "Size", "Traffic", "Date", "Notes"]:
        w("<th>%s&nbsp;&nbsp;&nbsp;</th>" % header)
    w("</tr>")
    w("</thead>")
    w("<tbody>")
    for campaign in campaigns.get_all("campaign"):
        v = campaign.get_text_val
        translations = campaign.get_all("translation")
        languages = [x.get_text_val("language") for x in translations]
        w("<tr>")
        icon = v("icon", "")
        if icon:
            tilde = icon.find("~")
            if tilde >= 0: icon = icon[:tilde]
            try: os.mkdir(path + "/icons")
            except OSError: pass
            if "." not in icon: icon += ".png"
            src = am_dir + "../../core/images/" + icon
            if os.path.exists(icon): src = icon
            if not os.path.exists(src):
                src = glob.glob(am_dir + "../../campaigns/*/images/" + icon)
                if src: src = src[0]
                if not src or not os.path.exists(src):
                    sys.stderr.write("Cannot find icon " + icon + "\n")
                    src = am_dir + "../../../images/misc/missing-image.png"
                    imgurl = "icons/missing-image.png"
                else:
                    imgurl = "icons/" + os.path.basename(icon)
            else:
                imgurl = "icons/" + os.path.basename(icon)
            command = os.path.join(am_dir, "../unit_tree/TeamColorizer '"
                + src + "' '" + path + "/" + imgurl + "'")
            os.system(command)
                
        type = v("type", "none")
        size = float(v("size", "0"))
        name = v("title", "unknown")
        w(('<td>%s</td>') % type)
        w(('<td><img alt="%s" src="%s" width="72px" height="72px"/>'
            ) % (icon, imgurl))
        w('<div class="desc"><b>%s</b><br/>%s</div></td>' % (
            name, v("description", "(no description)")))
        w("<td><b>%s</b><br/>" % name)
        w("Version: %s<br/>" % v("version", "unknown"))
        w("Author: %s</td>" % v("author", "unknown"))
        MiB = 1024 * 1024
        w("<td><span class=\"hidden\">%d</span><b>%.2f</b>MiB" % (size, size / MiB))
        if url:
            link = url.rstrip("/") + "/" + v("name") + ".tar.bz2"
            w("<br/><a href=\"%s\">download</a></td>" % link)
        else:
            w("</td>")
        downloads = int(v("downloads", "0"))
        w("<td><b>%d</b> down<br/>" % (downloads))
        w("%s up</td>" % v("uploads", "unknown"))
        timestamp = int(v("timestamp", "0"))
        t = time.localtime(timestamp)
        w("<td><span class=\"hidden\">%d</span>%s</td>" % (timestamp,
            time.strftime("%b %d %Y", t)))
        w("<td>%s</td>" % (", ".join(languages)))
        w("</tr>")
    w("</tbody>")
    w("</table>")

    w("""\
</div>
<div id="footer">
<p><a href="http://www.wesnoth.org/wiki/Site_Map">Site map</a></p>
<p><a href="http://www.wesnoth.org/wiki/Wesnoth:Copyrights">Copyright</a> &copy; 2003-2009 The Battle for Wesnoth</p>
<p>Supported by <a href="http://www.jexiste.fr/">Jexiste</a></p>
</div>
</body></html>""")
