#include "gettext.hpp"

#include <cstring>

const char* sgettext (const char *msgid)
{
	const char *msgval = gettext (msgid);
	if (msgval == msgid) {
		msgval = strrchr (msgid, '^');
		if (msgval == NULL)
			msgval = msgid;
		else
			msgval++;
	}
	return msgval;
}

const char* dsgettext (const char * domainname, const char *msgid)
{
	const char *msgval = dgettext (domainname, msgid);
	if (msgval == msgid) {
		msgval = strrchr (msgid, '^');
		if (msgval == NULL)
			msgval = msgid;
		else
			msgval++;
	}
	return msgval;
}

std::string vgettext (const char *msgid, const string_map& symbols)
{
	const std::string orig(gettext(msgid));
	const std::string msg = config::interpolate_variables_into_string(orig,
			&symbols);
	return msg;
}
