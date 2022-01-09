	/*************************************************************
	* Eddy is a simple live USB creation utility.                *
	*                                                            *
	* Authors                                                    *
	*                                                            *
	* Doug Yanez            <Deepspeed@bodhilinux.com>           *
	*                                                            *
	* Contributors:                                              *
	*                                                            *
	* Gareth Williams       <gareth.m.williams@gmail.com>        *
	*                                                            *
	* Robert Wiley          <ylee@bodhilinux.com>                *
	*                                                            *
	*                                                            *
	* Official upstream:  https://github.com/Deepspeed/eddy      *
	*                                                            *
	* Copyright: Doug Yanez 2021                                 *
	*                                                            *
	* License:  GPL v2                                           *
	*************************************************************/
//compile with
//  gcc -o eddy eddy.c `pkg-config --cflags --libs eina efl elementary eeze`

#include "eddy.h"

/* specific log domain to help debug only eddy */
int _eddy_log_dom = -1;
Evas_Object *hv;


/* function for child process to finish md5 check and show results properly */

static Eina_Bool
md5_msg_handler(void *data, int t EINA_UNUSED, void *event)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(event, ECORE_CALLBACK_DONE);
	EINA_SAFETY_ON_NULL_RETURN_VAL(data, ECORE_CALLBACK_DONE);
	Eddy_GUI * inst = data;
	Ecore_Exe_Event_Data *dataFromProcess = (Ecore_Exe_Event_Data *) event;
	int i, j = 0;
	char msg[BUFFER_SIZE], result[PATH_MAX];
	char str[] = "<align=left>Md5sum checked: ";

	if (dataFromProcess->size >= (BUFFER_SIZE - 1)){
		printf("Data too big for bugger. error\n");
		return ECORE_CALLBACK_DONE;
	}

	strncpy(msg, dataFromProcess->data, dataFromProcess->size);
	msg[dataFromProcess->size] = 0;
	printf("%s\n", msg);

	if (strcmp(msg, "quit") == 0){
		printf("My child said to me, QUIT!\n");
		ecore_main_loop_quit();
	}
	else{  //set results
		i = strlen(msg);
		while(msg[i] != ':')//iterate backwards to :
			i--;

		for(i += 2; msg[i] != '\0'; i++, j++)//store result separately
			result[j] = msg[i];

		strcat(str, result);//format text for label

		elm_object_text_set( inst->md5, str);//show test results
	}
	elm_progressbar_pulse(inst->busy, EINA_FALSE);
	return ECORE_CALLBACK_DONE;
}

/* return file extension of a file path */
static const char *
file_get_ext(const char *file)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(file, NULL);

	char *base = ecore_file_strip_ext(file);
	int i = strlen(base) + 1;
	free(base);
	return file + i*sizeof(char);
}

/* get the ISO selected and set it to a visible entry*/
static void
iso_chosen(void *data, Evas_Object *obj, void *event_info)
{
	EINA_SAFETY_ON_NULL_RETURN(event_info);
	EINA_SAFETY_ON_NULL_RETURN(data);
	EINA_SAFETY_ON_NULL_RETURN(data);

	Eddy_GUI *inst = data;
	const char *file = event_info;
	char buf[PATH_MAX];

	if (debug) INF("File: %s", file);
	// elm_fileselector_selected_set( obj, file);

	//filetype filter
	if(strcmp(file_get_ext(file), "iso")){
		printf("Wrong file type!  Try again.\n");
		elm_object_text_set(inst->iso,"<align=left>Please choose .iso file");
		return;
	}
	snprintf(buf, sizeof(buf), "<align=left>%s", file);
	elm_object_text_set(inst->iso, buf);
}

/* Check selected md5 file against ISO.  ISO must be in same folder. */

static void
md5_check(void *data, Evas_Object *o EINA_UNUSED, void *e)
{
	EINA_SAFETY_ON_NULL_RETURN(data);
	Eddy_GUI *inst = data;

	//ecore_exe stuff
	pid_t childPid;
	Ecore_Exe *childHandle;

	//filepaths
	char *isoPath = NULL;
	char md5Path[PATH_MAX+8];
	char *folderPath = NULL;
	char command[PATH_MAX+26];//terminal command

	const char *tmpPath = elm_object_text_get(inst->iso);
	if (debug) INF("ISO path %s", tmpPath);

	/* remove extra <align=left> bit */
	isoPath = elm_entry_markup_to_utf8(tmpPath);

	if(!strcmp(tmpPath, "<br>")){
		printf("No .iso file chosen yet!\n");
		elm_object_text_set(inst->md5, "<align=left>No .iso file chosen yet!");
		return;
	}

	elm_progressbar_pulse(inst->busy, EINA_TRUE);
	elm_object_text_set(inst->md5, "<align=left>Checking ISO md5sum");

	/* set folderPath directory */
	folderPath = ecore_file_dir_get(isoPath);

	//build terminal command
	snprintf(md5Path, PATH_MAX+7,  "%s.md5", isoPath);
	snprintf(command,PATH_MAX+25,"cd \"%s\" && md5sum -c \"%s\"",folderPath, md5Path);
	if (debug) INF("MD5 PATH %s", md5Path);
	free(isoPath);
	free(folderPath);

	//check for md5 file existence
	if(!ecore_file_exists(md5Path)){
		printf("md5 file does not exist in this folder!\n");
		elm_object_text_set(inst->md5, "<align=left>.md5 file not found!");
		elm_progressbar_pulse(inst->busy, EINA_FALSE);
		return;
	}

	//readability check
	if(!ecore_file_can_read(md5Path)){
		printf("md5 file cannot be read!\n");
		elm_object_text_set(inst->md5,"<align=left>.md5 file unreadable!");
		elm_progressbar_pulse(inst->busy, EINA_FALSE);
		return;
	}
  INF("COMMAND %s", command);
	/* execute md5 check. */
	childHandle = ecore_exe_pipe_run(command,
					ECORE_EXE_PIPE_WRITE |
					ECORE_EXE_PIPE_READ_LINE_BUFFERED |
					ECORE_EXE_PIPE_READ, NULL);

	if (childHandle == NULL){
		fprintf(stderr, "Could not create a child process!\n");
		elm_progressbar_pulse(inst->busy, EINA_FALSE);
		return;
	}

	childPid = ecore_exe_pid_get(childHandle);

	if (childPid == -1)
		fprintf(stderr, "Could not retrieve the PID!\n");
//	else
//		printf("Child process PID:%u\n",(unsigned int)childPid);

	ecore_event_handler_add(ECORE_EXE_EVENT_DATA, md5_msg_handler, data);
}

static void
usb_check(void *data, Evas_Object *o EINA_UNUSED, void *e)
{
	EINA_SAFETY_ON_NULL_RETURN(data);
	printf("USB Check\n");

	/* use ecore_exe to run badblocks command on chosen USB drive. */
}

static void
make_usb(void *data, Evas_Object *o EINA_UNUSED, void *e)
{
	EINA_SAFETY_ON_NULL_RETURN(data);
	printf("make_usb running\n");

	/* use ecore_exe to install the chosen iso from entry1
	 * onto the selected drive from the genlist using dd
	 */
}

/* help window */
static void
help_info(void *data EINA_UNUSED,Evas_Object *object EINA_UNUSED,void *event_info)
{
   Evas_Object *help, *scroller, *label;

	/* help main window */
	help = elm_win_util_standard_add("Help", "Help");
	elm_win_autodel_set(help, EINA_TRUE);
	evas_object_size_hint_min_set(help,270,150); //min size not working??

	label = elm_label_add(help);
	elm_object_text_set(label, "To perform an MD5sum check on your iso,<br>\
	both files (.iso and .md5) MUST be in the<br>\
	same folder and you must select an iso.<br><br>\
	Opening Eddy from the command line using: <br>\
	./eddy -f ~/path/to/iso-file.iso<br>\
	will fill the iso field in Eddy<br><br>\
	Expect more features as time goes on.");
	efl_gfx_hint_align_set(label,0.0,1);
	evas_object_show(label);

	scroller = elm_scroller_add(help);
	evas_object_size_hint_weight_set(scroller, 
					  EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(help, scroller);
	evas_object_show(scroller);
	elm_object_content_set(scroller, label);
	elm_scroller_bounce_set(scroller, EINA_TRUE, EINA_FALSE);
	elm_scroller_policy_set(scroller, 
				ELM_SCROLLER_POLICY_ON, ELM_SCROLLER_POLICY_ON);
	elm_scroller_propagate_events_set(scroller, EINA_TRUE);
	elm_scroller_page_relative_set(scroller, 0, 1);
	elm_scroller_region_show(scroller, 50, 50, 200, 200);
	evas_object_resize(help, 270,150);

	evas_object_show(help);
}

static void find_drives()
{
	//this needs to be way better.  Gotta use more eeze stuff.
	/*
	useful commands:

	eeze_disk_devpath_get (Eeze_Disk *disk)
	eeze_disk_fstype_get (Eeze_Disk *disk)
	eeze_disk_label_get (Eeze_Disk *disk)
	eeze_disk_mounted_get (Eeze_Disk *disk)
	eeze_disk_unmount (Eeze_Disk *disk)
	eeze_disk_mount_point_get (Eeze_Disk *disk)

	*/
	
	//Need drive size, /dev/sdx address, filesystem, and mounted state
	Eina_List *drives = eeze_udev_find_by_type(EEZE_UDEV_TYPE_DRIVE_REMOVABLE,
						    NULL);
	const char *drv;

	EINA_LIST_FREE(drives,drv){
//		printf("DRIVE: %s\n",drv);
		elm_hoversel_item_add(hv,drv,NULL,ELM_ICON_NONE,NULL,drv);
		eina_stringshare_del(drv);//free them as we list them in hv
	}
}


EAPI_MAIN int elm_main(int argc, char **argv)
{
	_eddy_log_dom = eina_log_domain_register("eddy", EDDY_DEFAULT_LOG_COLOR);
	if (_eddy_log_dom < 0)
	{
		EINA_LOG_ERR("Unable to create a log domain.");
		exit(-1);
	}
	eina_log_domain_level_set("eddy", EINA_LOG_LEVEL_INFO);
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
#ifdef ENABLE_NLS
   elm_app_compile_locale_set(LOCALEDIR);
#endif
   elm_app_info_set(elm_main, "eddy", "COPYING");

   setlocale(LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALE_DIR);
   textdomain(PACKAGE);

	Evas_Object *win, *table;
	Evas_Object *iso_bt, *md5_check_bt, *usb_check_bt, *dd_bt, *help_bt;
	// Elm_Genlist_Item_Class *glist;
	Eddy_GUI *inst = calloc(1, sizeof(Eddy_GUI));
	EINA_SAFETY_ON_NULL_RETURN_VAL(inst, -1);

	/* look for and perform any cli args
	 * will run with './eddy -f /path/to/file.iso' */
	char path[PATH_MAX]; 
	int hold = -10;

	for (int i=0; argv[i] != NULL; i++){
		if(strcmp(argv[i], "-f") == 0){
			hold = i+1;
			snprintf(path,sizeof(path),"<align=left>%s",argv[hold]);
		}
		if(strcmp(argv[i], "-d") == 0)
		{
			debug = EINA_TRUE;
			INF("DEBUG ON");
		}
		if(strcmp(argv[i], "-h") == 0){
			printf("Help not implemented here yet.  Good luck!\n");
			return 0;
		}
	}

	//set up window
	win = elm_win_util_standard_add("main", _("Eddy - Live USB Utility"));
	elm_win_autodel_set(win, EINA_TRUE);

	elm_app_name_set("Eddy");

	/* table, replacing grid */
	table = elm_table_add(win);
	elm_table_padding_set(table, 5, 5);
	elm_table_align_set(table,0.0,0.5);
	evas_object_size_hint_weight_set(table,EVAS_HINT_EXPAND,EVAS_HINT_EXPAND);
	elm_win_resize_object_add(win,table);
	evas_object_show(table);


	/* ISO selector button */
	iso_bt = elm_fileselector_button_add(table);
	elm_fileselector_button_inwin_mode_set(iso_bt, EINA_TRUE);
	elm_fileselector_expandable_set(iso_bt, EINA_FALSE);
	elm_fileselector_folder_only_set(iso_bt, EINA_FALSE);
	elm_fileselector_is_save_set(iso_bt, EINA_TRUE);
	elm_object_text_set(iso_bt, _("Select ISO"));
	evas_object_size_hint_align_set(iso_bt, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table,iso_bt,0,0,1,1);
	evas_object_show(iso_bt);


	/* ISO text label */
	inst->iso = elm_label_add(table);
	elm_label_ellipsis_set(inst->iso, EINA_TRUE);
	evas_object_size_hint_weight_set(inst->iso, EVAS_HINT_EXPAND, 0.0);
	evas_object_size_hint_align_set(inst->iso, EVAS_HINT_FILL, 0.0);

	/* label is created, now we can assign the argv to the label. */
	if (hold != -10){  //if it has changed
		elm_object_text_set(inst->iso, path);
	}
	else elm_object_text_set(inst->iso, "\0");

	elm_table_pack(table, inst->iso, 1,0,5,1);
	evas_object_show(inst->iso);


	/* Md5 Check Button */
	md5_check_bt = elm_button_add(table);
	elm_button_autorepeat_set(md5_check_bt, EINA_FALSE);
	elm_object_text_set(md5_check_bt, _("Check md5"));
	evas_object_size_hint_align_set(md5_check_bt, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table,md5_check_bt,0,1,1,1);
	evas_object_show(md5_check_bt);


	/* md5 text label */
	inst->md5 = elm_label_add(table);
	elm_label_ellipsis_set(inst->md5, EINA_TRUE);
	evas_object_size_hint_weight_set(inst->md5, EVAS_HINT_EXPAND, 1.0);
	evas_object_size_hint_align_set(inst->md5, EVAS_HINT_FILL, 0.0);

	elm_table_pack(table, inst->md5, 1,1,5,1);
	evas_object_show(inst->md5);


	/* USB chooser hoversel */
	hv = elm_hoversel_add(win);
	elm_hoversel_hover_parent_set(hv, win);
	elm_hoversel_horizontal_set(hv, EINA_FALSE);
	elm_object_text_set(hv, _("Choose a drive"));
	elm_hoversel_auto_update_set(hv, EINA_TRUE);
	
	elm_table_pack(table, hv, 0,3,1,1);
	evas_object_show(hv);
	
	find_drives();
	
	/* USB Check Button */
	usb_check_bt = elm_button_add(table);
	elm_button_autorepeat_set(usb_check_bt, EINA_FALSE);
	elm_object_text_set(usb_check_bt, _("Check USB"));
	evas_object_size_hint_align_set(usb_check_bt, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table,usb_check_bt,0,4,1,1);
	evas_object_show(usb_check_bt);


	/* USB result label */
	inst->usb = elm_label_add(table);
	elm_label_ellipsis_set(inst->usb, EINA_TRUE);
	evas_object_size_hint_weight_set(inst->usb, EVAS_HINT_EXPAND, 0.0);
	evas_object_size_hint_align_set(inst->usb, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table, inst->usb, 1,4,5,1);
	evas_object_show(inst->usb);


	/* DD buttom */
	dd_bt = elm_button_add(table);
	elm_button_autorepeat_set(dd_bt, EINA_FALSE);
	elm_object_text_set(dd_bt, _("Make Live USB"));
	evas_object_size_hint_align_set(dd_bt, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table,dd_bt,0,12,1,1);
	evas_object_show(dd_bt);


	/* help button */
	help_bt = elm_button_add(table);
	elm_button_autorepeat_set(help_bt, EINA_FALSE);
	elm_object_text_set(help_bt, "?");
	evas_object_size_hint_align_set(help_bt, EVAS_HINT_FILL, 0.5);

	elm_table_pack(table,help_bt,5,13,1,1);
	evas_object_show(help_bt);


	/* progress bar */

	//totally broken... Pulsing isn't working at all.
	inst->busy = elm_progressbar_add(table);
	evas_object_size_hint_align_set(inst->busy, EVAS_HINT_FILL, 0.5);
	evas_object_size_hint_weight_set(inst->busy, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_progressbar_pulse_set(inst->busy, EINA_TRUE);
	elm_progressbar_unit_format_set(inst->busy, NULL);

	elm_table_pack(table,inst->busy, 0,14,6,1);
	evas_object_show(inst->busy);

	//add callbacks for buttons
	evas_object_smart_callback_add(iso_bt, "file,chosen", iso_chosen, inst);
	evas_object_smart_callback_add(md5_check_bt, "clicked", md5_check, inst);
	evas_object_smart_callback_add(usb_check_bt, "clicked", usb_check, inst);
	evas_object_smart_callback_add(dd_bt,"clicked", make_usb, inst);
	evas_object_smart_callback_add(help_bt, "clicked", help_info, NULL);

	/* set final window size and display it */
	evas_object_resize(win, ELM_SCALE_SIZE(420), ELM_SCALE_SIZE(300));
	evas_object_show(table);  //is this needed?
	evas_object_show(win);

	elm_run();
	eina_log_domain_unregister(_eddy_log_dom);
	_eddy_log_dom = -1;
	return 0;
}
ELM_MAIN()
