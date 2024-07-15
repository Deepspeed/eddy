#ifndef EDDY_H
# define EDDY_H

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include "../config.h"
#include <Elementary.h>
#include <Eeze.h>
#include <time.h>
#include <string.h>

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(x) gettext(x)
#else
# define _(x) (x)
#endif

#define BUFFER_SIZE 1024

extern int _eddy_log_dom;
#define EDDY_DEFAULT_LOG_COLOR EINA_COLOR_CYAN

#define CRI(...)      EINA_LOG_DOM_CRIT(_eddy_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_eddy_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_eddy_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_eddy_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_eddy_log_dom, __VA_ARGS__)

typedef struct _Eddy_GUI
{
	Evas_Object *win;
	Evas_Object *entry;
	Evas_Object *busy;
	Evas_Object *iso;
	Evas_Object *usb;
	Evas_Object *md5;

	char *md5Path;
	char *md5Command;
} Eddy_GUI;

Eina_Bool debug=EINA_FALSE;

#endif
