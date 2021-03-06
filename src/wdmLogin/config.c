/*
 * wdm - WINGs display manager
 * Copyright (C) 2003 Alexey Voinov <voins@voins.program.ru>
 * Copyright (C) 1998 Gene Czarcinski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * config.c: read configuration for wdmLogin
 */
#include <wdmconfig.h>
#include <wdmlib.h>
#include <wdmLogin.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static Bool WDMCheckPLGeometry(WMPropList * pl, void *def, void *target);

static WMRect default_geometry = { {INT_MIN, INT_MIN}, {530, 240} };

static WDMDictionaryStruct wdmLogin_config_struct[] = {
	{"geometry", WDMCheckPLGeometry, &default_geometry,
	 offsetof(WDMLoginConfig, geometry)},
#ifdef USE_AA
	{"aa", WDMCheckPLBool, (void *)False,
	 offsetof(WDMLoginConfig, aaenabled)},
	{"multibyte", WDMCheckPLBool, (void *)True,
	 offsetof(WDMLoginConfig, multibyte)},
#endif
	{"animations", WDMCheckPLBool, False,
	 offsetof(WDMLoginConfig, animations)},
	{NULL, NULL, NULL, 0}
};

static WDMDictionarySpec wdmLogin_config = { sizeof(WDMLoginConfig), wdmLogin_config_struct };

static Bool WDMCheckPLInteger(WMPropList * pl, void *def, void *target)
{
	int *int_target = (int *)target;
	int int_def = (int)(intptr_t)def;
	char *text = NULL;
	char *endptr = NULL;

	WDMDebug("WDMCheckPLInteger(%p, %p, %p)\n", (void *)pl, def, target);

	*int_target = int_def;
	if (pl && WMIsPLString(pl)) {
		text = WMGetFromPLString(pl);
		if (text != NULL)
			*int_target = strtol(text, &endptr, 0);
	}
	return True;
}

static Bool WDMCheckPLUInteger(WMPropList * pl, void *def, void *target)
{
	unsigned int *int_target = (unsigned int *)target;
	unsigned int int_def = (unsigned int)(intptr_t)def;
	char *text = NULL;
	char *endptr = NULL;

	WDMDebug("WDMCheckPLUInteger(%p, %p, %p)\n", (void *)pl, def, target);

	*int_target = int_def;
	if (pl && WMIsPLString(pl)) {
		text = WMGetFromPLString(pl);
		if (text != NULL)
			*int_target = strtoul(text, &endptr, 0);
	}
	return True;
}

static Bool WDMCheckPLGeometry(WMPropList * pl, void *def, void *target)
{
	WMRect *rect_target = (WMRect *) target;

	WDMDebug("WDMCheckPLGeometry(%p, %p, %p)\n", (void *)pl, def, target);

	if (def)
		memcpy(rect_target, def, sizeof(WMRect));

	if (pl != NULL && WMIsPLArray(pl)) {
		WDMCheckPLUInteger(WMGetFromPLArray(pl, 0), (void *)(intptr_t)rect_target->size.width, &rect_target->size.width);
		WDMCheckPLUInteger(WMGetFromPLArray(pl, 1), (void *)(intptr_t)rect_target->size.height, &rect_target->size.height);
		WDMCheckPLInteger(WMGetFromPLArray(pl, 2), (void *)(intptr_t)rect_target->pos.x, &rect_target->pos.x);
		WDMCheckPLInteger(WMGetFromPLArray(pl, 3), (void *)(intptr_t)rect_target->pos.y, &rect_target->pos.y);
		return True;
	}
	return False;
}

WDMLoginConfig *LoadConfiguration(char *configFile)
{
	char *filename = configFile ? configFile : DEF_WDMLOGIN_CONFIG;
	WMPropList *db;
	WDMLoginConfig *config = NULL;

	db = WMReadPropListFromFile(filename);
	if (db == NULL)
		WDMError("Cannot open config file. Using builtin defaults\n");
	if (!WDMCheckPLDictionary(db, &wdmLogin_config, &config))
		WDMError("Error parsing config file. Using builtin defaults\n");
	if (db)
		WMReleasePropList(db);
	return config;
}
