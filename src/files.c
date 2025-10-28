/*
 * Copyright (C) 2000-2002 Misha Nasledov <misha@nasledov.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "hx.h"
#include "gtk_hlist.h"
#include "macres.h"
#include "xfers.h"
#include "toolbar.h"
#include "gtkutil.h"
#include "gtkhx.h"
#include "cicn.h"
#include "tasks.h"
#include "rcv.h"
#include "files.h"
#include "pixmaps/refresh.xpm"
#include "pixmaps/ul.xpm"
#include "pixmaps/dl.xpm"
#include "pixmaps/mkdir.xpm"
#include "pixmaps/trash.xpm"
#include "pixmaps/info.xpm"
#include "pixmaps/up.xpm"


#define ICON_FILE	400
#define ICON_FOLDER	401
#define ICON_FOLDER_IN	421
#define ICON_FILE_HTft	402
#define ICON_FILE_SIT	403
#define ICON_FILE_TEXT	404
#define ICON_FILE_IMAGE	406
#define ICON_FILE_APPL	407
#define ICON_FILE_HTLC	408
#define ICON_FILE_SITP	409
#define ICON_FILE_alis	422
#define ICON_FILE_DISK	423
#define ICON_FILE_NOTE	424
#define ICON_FILE_MOOV	425
#define ICON_FILE_ZIP	426

guint8 dir_char  = '/';

struct gfile_list *gfile_list;

static struct gfile_list *gfl_new (GtkWidget *window, GtkWidget *hlist,
								   char *path)
{
	struct gfile_list *gfl;

	gfl = g_malloc(sizeof(struct gfile_list));
	gfl->next = 0;
	gfl->prev = 0;

	if (gfile_list) {
		gfile_list->next = gfl;
		gfl->prev = gfile_list;
	}

	gfl->cfl = 0;
	gfl->window = window;
	gfl->hlist = hlist;
	gfl->row = 0;
	gfl->column = 0;

	gfl->path_list = g_malloc(sizeof(struct path_hist)+strlen(path));
	strcpy(gfl->path_list->path, path);
	gfl->path_list->prev = NULL;

	gfl->in_use = 0;

	gfile_list = gfl;

	return gfl;
}

static void gfl_delete (struct gfile_list *gfl)
{
	struct path_hist *path, *prev;

	for(path = gfl->path_list; path; path = prev) {
		if(path->prev) {
			prev = path->prev;
		}
		else {
			prev = 0;
		}
		g_free(path);
	}

	g_free(gfl->cfl);

	if (gfl->next)
		gfl->next->prev = gfl->prev;
	if (gfl->prev)
		gfl->prev->next = gfl->next;
	if (gfl == gfile_list)
		gfile_list = gfl->prev;
	g_free(gfl);
}

void destroy_gfl_list(void)
{
	struct gfile_list *gfl, *prev;


	for(gfl = gfile_list; gfl; gfl = prev) {
		prev = gfl->prev;
		gtk_widget_destroy(gfl->window);
		gfl_delete(gfl);
	}
	gfile_list = 0;
}

static struct gfile_list *gfl_with_hlist (GtkWidget *hlist)
{
	struct gfile_list *gfl;

	for(gfl = gfile_list; gfl; gfl = gfl->prev) {
		if(gfl->hlist == hlist) {
			return gfl;
		}
	}

	return 0;
}

static struct gfile_list *gfl_with_path (char *path)
{
	struct gfile_list *gfl;

	for(gfl = gfile_list; gfl; gfl = gfl->prev) {
		if(!strcmp(path, gfl->cfl->path)) {
			return gfl;
		}
	}

	return 0;
}

static void
open_fldr (struct cached_filelist *cfl, struct hl_filelist_hdr *fh,
		   struct gfile_list *gfl)
{
	char path[4096];
	char *curr_path = g_strdup_printf("%.*s", (int)fh->fnlen, fh->fname);

	if(gfl->in_use) {
		g_free(curr_path);
		return;
	}

	if(cfl->path[0] == '/' && cfl->path[1] == 0) {
		snprintf(path, sizeof(path), "/%s", curr_path);
	}
	else {
		snprintf(path, sizeof(path), "%s/%s", cfl->path, curr_path);
	}
	g_free(curr_path);


	gfl->row = 0;
	hx_list_dir(&sessions[0].htlc, path, 1, 0, gfl);
}

static void
get_file (struct cached_filelist *cfl, struct hl_filelist_hdr *fh)
{
	char rpath[4096], lpath[4096];
	struct htxf_conn *htxf;
	struct stat sb;

	snprintf(rpath, sizeof(rpath), "%s/%.*s", cfl->path, (int)fh->fnlen,
			fh->fname);
	snprintf(lpath, sizeof(lpath), "%s/%.*s", gtkhx_prefs.download_path,
			(int)fh->fnlen, fh->fname);

	if(stat(gtkhx_prefs.download_path, &sb)) {
		if(mkdir(gtkhx_prefs.download_path, 0770)) {
			g_warning("%s: %s", _("cannot create download directory"),
					  gtkhx_prefs.download_path);
			g_warning(_("aborting download"));
			return;
		}
	}


	htxf = xfer_new(lpath, rpath, XFER_GET);
	htxf->filter_argv = 0;
	htxf->opt.retry = 0;
}

static void put_file(GtkWidget *widget, gpointer data)
{
	char *lpath = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
	GtkWidget *files_list = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget),
															 "files_list");
	struct gfile_list *gfl;
	char rpath[4096];

	gfl = gfl_with_hlist(files_list);
	if(!gfl || !gfl->cfl) {
		return;
	}

	snprintf(rpath, sizeof(rpath), "%s/%s", gfl->cfl->path, basename(lpath));

	hx_put_file(&sessions[0].htlc, lpath, rpath);
	gtk_widget_destroy(GTK_WIDGET(data));

}

static void get_put_data (GtkWidget *widget, gpointer data)
{
	GtkWidget *file_dialog = gtk_file_selection_new(_("Upload..."));

	gtk_signal_connect_object(GTK_OBJECT(file_dialog), "destroy",
							  (GtkSignalFunc)gtk_widget_destroy,
							  GTK_OBJECT(file_dialog));
	gtk_signal_connect_object(GTK_OBJECT(
								  GTK_FILE_SELECTION(
									  file_dialog)->cancel_button),
							  "clicked", (GtkSignalFunc) gtk_widget_destroy,
							  GTK_OBJECT(file_dialog));
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(file_dialog)->ok_button),
					   "clicked", GTK_SIGNAL_FUNC(put_file), file_dialog);

	gtk_object_set_data(GTK_OBJECT(GTK_FILE_SELECTION(file_dialog)->ok_button),
						"files_list", data);

	gtk_widget_show_all(file_dialog);
	gtk_grab_add(file_dialog);
}

static void file_clicked (GtkWidget *widget, GdkEventButton *event)
{
	struct gfile_list *gfl;
	int row;
	int column;

	gfl = gfl_with_hlist(widget);

	if (!gfl)
		return;

	gtk_hlist_get_selection_info(GTK_HLIST(widget),
				     event->x, event->y, &row, &column);

	if (event->type == GDK_2BUTTON_PRESS) {
		struct hl_filelist_hdr *fh;

		fh = gtk_hlist_get_row_data(GTK_HLIST(widget), gfl->row);
		if (fh) {
			if(gfl->cfl) {
				if (!memcmp(&fh->ftype, "fldr", 4)) {
					open_fldr(gfl->cfl, fh, gfl);
				} else {
					get_file(gfl->cfl, fh);
				}
			}
		}
	}
	else {
		gfl->row = row;
		gfl->column = column;
	}
}

static void delete_file (GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;
	char path[4096];

	gfl = gfl_with_hlist((GtkWidget *)data);
	fh = gtk_hlist_get_row_data(GTK_HLIST(data), gfl->row);
	

	snprintf(path, sizeof(path), "%s/%.*s", gfl->cfl->path,
			 (int)fh->fnlen, fh->fname);
	hx_file_delete(&sessions[0].htlc, path);
	hx_list_dir(&sessions[0].htlc, gfl->cfl->path, 1, 0, gfl);
 }

static void get_file_info(GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;
	char path[4096];

	gfl = gfl_with_hlist((GtkWidget *)data);
	fh = gtk_hlist_get_row_data(GTK_HLIST(data), gfl->row);

	sprintf(path, "%s/%.*s", gfl->cfl->path,
			 (int)fh->fnlen, fh->fname);

	hx_file_info(&sessions[0].htlc, path);
}

static void file_up_btn (GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl;
	struct path_hist *path;

	if(!gtkhx_prefs.file_samewin) {
		return;
	}

	gfl = gfl_with_hlist((GtkWidget *)data);

	if(!gfl) {
		return;
	}

	if(gfl->in_use) {
		return;
	}

	if(gfl->path_list->prev) {
		path = gfl->path_list;
		gfl->path_list = gfl->path_list->prev;
		g_free(path);
	}
	else {
		return;
	}


	gfl->in_use = 1;
	gfl->row = 0;
	hx_list_dir(&sessions[0].htlc, gfl->path_list->path, 1, 0, gfl);
}

static void file_dl_btn (GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;
	char rpath[4096], lpath[4096];
	struct htxf_conn *htxf;

	gfl = gfl_with_hlist((GtkWidget *)data);
	fh = gtk_hlist_get_row_data(GTK_HLIST(data), gfl->row);

	snprintf(rpath, sizeof(rpath), "%s/%.*s", gfl->cfl->path,
			 (int)fh->fnlen, fh->fname);

	snprintf(lpath, sizeof(lpath), "%s/%.*s", gtkhx_prefs.download_path,
			 (int)fh->fnlen, fh->fname);

	if(!memcmp(&fh->ftype, "fldr", 4)) {
		return;
	}

	htxf = xfer_new(lpath, rpath, XFER_GET);
	htxf->filter_argv = 0;
	htxf->opt.retry = 0;
}

static void file_pre_btn (GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl;
	struct hl_filelist_hdr *fh;
	char rpath[4096], lpath[4096];
	struct htxf_conn *htxf;

	gfl = gfl_with_hlist((GtkWidget *)data);
	fh = gtk_hlist_get_row_data(GTK_HLIST(data), gfl->row);

	snprintf(rpath, sizeof(rpath), "%s/%.*s", gfl->cfl->path,
			 (int)fh->fnlen, fh->fname);

	snprintf(lpath, sizeof(lpath), "%s/%.*s", gtkhx_prefs.download_path,
			 (int)fh->fnlen, fh->fname);

	if(!memcmp(&fh->ftype, "fldr", 4)) {
		return;
	}

	htxf = xfer_new(lpath, rpath, XFER_GET);
	htxf->filter_argv = 0;
	htxf->opt.retry = 0;
	htxf->opt.preview = 1;
}


static void file_reload_btn (GtkWidget *widget, gpointer data)
{
	GtkWidget *files_list = (GtkWidget *)data;
	struct gfile_list *gfl;

	gfl = gfl_with_hlist(files_list);

	if(!gfl->cfl) {
		return;
	}

	gtk_hlist_clear(GTK_HLIST(files_list));
	hx_list_dir(&sessions[0].htlc, gfl->cfl->path, 1, 0, gfl);
}

static void close_files_window (GtkWidget *widget, gpointer data)
{
	struct gfile_list *gfl = (struct gfile_list *)gtk_object_get_data(
		GTK_OBJECT(widget), "gfl");

	gfl_delete(gfl);
	gtk_widget_destroy(widget);
}

static void makeDir(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget),
														"entry");
	GtkWidget *files_list = (GtkWidget *)gtk_object_get_data(GTK_OBJECT(widget),
															 "hlist");
	char pathname[MAXPATHLEN];
	struct gfile_list *gfl = gfl_with_hlist(files_list);


	snprintf(pathname, MAXPATHLEN, "%s/%s", gfl->cfl->path, gtk_entry_get_text(GTK_ENTRY(entry)));
	hx_make_dir(&sessions[0].htlc, pathname);
	hx_list_dir(&sessions[0].htlc, gfl->cfl->path, 1, 0, gfl);

	gtk_widget_destroy(GTK_WIDGET(data));
}

static void makeDirDialog(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *nameEntry;
	GtkWidget *okBtn;
	GtkWidget *cancelBtn;
	GtkWidget *nameEntryLabel;
	GtkWidget *entryHbox;
	GtkWidget *btnHbox;

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("New Folder..."));
	entryHbox = gtk_hbox_new(0,0);

    gtk_container_border_width (GTK_CONTAINER(dialog), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entryHbox ,0, 0, 0);
	nameEntryLabel = gtk_label_new(_("Name: "));
	nameEntry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(entryHbox), nameEntryLabel, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(entryHbox), nameEntry, 0, 0, 0);

	okBtn = gtk_button_new_with_label(_("OK"));
	gtk_object_set_data(GTK_OBJECT(okBtn), "entry", nameEntry);
	gtk_object_set_data(GTK_OBJECT(okBtn), "hlist", data);
	gtk_signal_connect(GTK_OBJECT(okBtn), "clicked", GTK_SIGNAL_FUNC(makeDir),
					   dialog);

	cancelBtn = gtk_button_new_with_label(_("Cancel"));
	gtk_signal_connect_object(GTK_OBJECT(cancelBtn), "clicked",
							  (GtkSignalFunc) gtk_widget_destroy,
							  GTK_OBJECT(dialog));

	btnHbox = gtk_hbox_new(0,0);
	GTK_WIDGET_SET_FLAGS(okBtn, GTK_CAN_DEFAULT);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), btnHbox);
	gtk_box_pack_start(GTK_BOX(btnHbox), okBtn, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(btnHbox), cancelBtn, 0, 0, 0);
	gtk_widget_show_all(dialog);
	gtk_widget_grab_default(okBtn);
}

static GtkTargetEntry files_drag[] =
{{"hlist", GTK_TARGET_SAME_APP, 0}};

static void files_drag_send(GtkWidget *source, GdkDragContext *context,
							GtkSelectionData *selection, guint targetType,
							guint eventTime)
{
	/* We are the knights who say "Nee!" */
	gtk_selection_data_set(selection, selection->type,  0, 0, 0);
}

static void files_drag_receive(GtkWidget *target, GdkDragContext *context,
							   int x, int y, GtkSelectionData *selection,
							   guint targetType, guint time, gpointer data)
{
	GtkWidget *source = gtk_drag_get_source_widget(context);
	struct gfile_list *gfl_source, *gfl_target;
	struct hl_filelist_hdr *fh;
	char pathf[4096], patht[4096];

	gfl_source = gfl_with_hlist(source);
	fh = gtk_hlist_get_row_data(GTK_HLIST(source), gfl_source->row);
	if(!fh) {
		return;
	}

	gfl_target = gfl_with_hlist(target);

	if(strcmp(gfl_source->cfl->path, gfl_target->cfl->path)) {
		sprintf(pathf, "%s/%.*s", gfl_source->cfl->path, (int)fh->fnlen,
				fh->fname);
		sprintf(patht, "%s/", gfl_target->cfl->path);

		hx_file_move(&sessions[0].htlc, pathf, patht);
/*		hx_file_link(&sessions[0].htlc, pathf, patht);

		XXX: Pop up a dialog and prompt the user whether he wants to move 
		or link the file or cancel */

		hx_list_dir(&sessions[0].htlc, gfl_target->cfl->path, 1, 0, 
		gfl_target); 
		hx_list_dir(&sessions[0].htlc, gfl_source->cfl->path, 1, 0, 
		gfl_source); 
	}
}

static struct gfile_list *create_files_window (char *path)
{
	GtkWidget *files_window;
	GtkWidget *files_list;
	GtkWidget *files_window_scroll;
	GtkWidget *reloadbtn;
	GtkWidget *downloadbtn;
	GtkWidget *crtfldbtn;
	GtkWidget *filinfobtn;
	GtkWidget *uploadbtn;
	GtkWidget *delbtn;
	GtkWidget *upbtn;
	GtkWidget *prebtn;
	GtkWidget *vbox;
	GtkWidget *hbuttonbox;
	GtkWidget *topframe;
	GdkBitmap *mask;
	GdkPixmap *icon;
	GtkWidget *pix;
	GtkStyle *style;
	struct gfile_list *gfl;
	gchar *titles[2];

	titles[0] = _("Size");
	titles[1] = _("Name");

	files_list = gtk_hlist_new_with_titles(2, titles);
	gtk_hlist_set_column_width(GTK_HLIST(files_list), 0, 64);
	gtk_hlist_set_column_width(GTK_HLIST(files_list), 1, 240);
	gtk_hlist_set_row_height(GTK_HLIST(files_list), 18);
	gtk_hlist_set_shadow_type(GTK_HLIST(files_list), GTK_SHADOW_NONE);
	gtk_hlist_set_column_justification(GTK_HLIST(files_list), 0,
									   GTK_JUSTIFY_LEFT);
	gtk_signal_connect(GTK_OBJECT(files_list), "button_press_event",
					   GTK_SIGNAL_FUNC(file_clicked), 0);

	gtk_signal_connect(GTK_OBJECT(files_list), "drag_data_get",
					   GTK_SIGNAL_FUNC(files_drag_send), 0);
	gtk_drag_source_set(files_list, GDK_BUTTON1_MASK, files_drag, 1,
						GDK_ACTION_MOVE);

	gtk_signal_connect(GTK_OBJECT(files_list), "drag_data_received",
					   GTK_SIGNAL_FUNC(files_drag_receive), 0);
	gtk_drag_dest_set(files_list, GTK_DEST_DEFAULT_ALL, files_drag, 1,
					  GDK_ACTION_MOVE|GDK_ACTION_LINK);

	files_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(files_window), "files", "GtkHx");
	gtk_window_set_policy(GTK_WINDOW(files_window), 1, 1, 0);

	gtk_widget_realize(files_window);
	style = gtk_widget_get_style(files_window);
	gtk_window_set_title(GTK_WINDOW(files_window), path);
	gtk_widget_set_usize(files_window, 264, 400);

	gfl = gfl_new(files_window, files_list, path);
	gtk_object_set_data(GTK_OBJECT(files_window), "gfl", gfl);
	gtk_signal_connect(GTK_OBJECT(files_window), "delete_event",
			   GTK_SIGNAL_FUNC(close_files_window), files_list);

	files_window_scroll = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(files_window_scroll),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	topframe = gtk_frame_new(0);
	gtk_widget_set_usize(topframe, -2, 30);
	gtk_frame_set_shadow_type(GTK_FRAME(topframe), GTK_SHADOW_OUT);

	hbuttonbox = gtk_hbox_new(0,0);

	upbtn = gtk_button_new();
	gfl->up_btn = upbtn;
	gtk_signal_connect(GTK_OBJECT(upbtn), "clicked",
					   GTK_SIGNAL_FUNC(file_up_btn), files_list);
	gtk_tooltips_set_tip(tooltips, upbtn, _("Parent Directory"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL], up_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(upbtn), pix);
	pix = 0, icon = 0, mask = 0;

	reloadbtn =  gtk_button_new();
	gtk_signal_connect(GTK_OBJECT(reloadbtn), "clicked",
					   GTK_SIGNAL_FUNC(file_reload_btn), files_list);
	gtk_tooltips_set_tip(tooltips, reloadbtn, _("Reload"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL],
										refresh_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(reloadbtn), pix);
	pix = 0, icon = 0, mask = 0;

	downloadbtn = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, downloadbtn, _("Download"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL], dl_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_signal_connect(GTK_OBJECT(downloadbtn), "clicked",
					   GTK_SIGNAL_FUNC(file_dl_btn), files_list);
	gtk_container_add(GTK_CONTAINER(downloadbtn), pix);
	pix = 0, icon = 0, mask = 0;

	prebtn = gtk_button_new_with_label("[ P ]");
	gtk_tooltips_set_tip(tooltips, prebtn, _("Preview"), 0);
	gtk_signal_connect(GTK_OBJECT(prebtn), "clicked",
					   GTK_SIGNAL_FUNC(file_pre_btn), files_list);

	uploadbtn = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, uploadbtn, _("Upload"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL], ul_xpm);
	gtk_signal_connect(GTK_OBJECT(uploadbtn), "clicked",
					   GTK_SIGNAL_FUNC(get_put_data), files_list);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(uploadbtn), pix);
	pix = 0, icon = 0, mask = 0;

	crtfldbtn = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, crtfldbtn, _("New Folder"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL],
										mkdir_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(crtfldbtn), pix);
	gtk_signal_connect(GTK_OBJECT(crtfldbtn), "clicked",
					   GTK_SIGNAL_FUNC(makeDirDialog), files_list);
	pix = 0, icon = 0, mask = 0;

	filinfobtn = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, filinfobtn, _("Info"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL], info_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(filinfobtn), pix);
	gtk_signal_connect(GTK_OBJECT(filinfobtn), "clicked",
					   GTK_SIGNAL_FUNC(get_file_info), files_list);
	pix = 0, icon = 0, mask = 0;

	delbtn = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, delbtn, _("Delete"), 0);
	icon = gdk_pixmap_create_from_xpm_d(toolbar_window->window, &mask,
										&style->bg[GTK_STATE_NORMAL],
										trash_xpm);
	pix = gtk_pixmap_new(icon, mask);
	gtk_container_add(GTK_CONTAINER(delbtn), pix);
	gtk_signal_connect(GTK_OBJECT(delbtn), "clicked",
					   GTK_SIGNAL_FUNC(delete_file), files_list);
	pix = 0, icon = 0, mask = 0;


	vbox = gtk_vbox_new(0, 0);
	gtk_widget_set_usize(vbox, 240, 400);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), upbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), reloadbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), downloadbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), uploadbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), crtfldbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), filinfobtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), delbtn, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(hbuttonbox), prebtn, 0, 0, 2);

	gtk_container_add(GTK_CONTAINER(topframe), hbuttonbox);
	gtk_box_pack_start(GTK_BOX(vbox), topframe, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(files_window_scroll), files_list);
	gtk_box_pack_start(GTK_BOX(vbox), files_window_scroll, 1, 1, 0);
	gtk_container_add(GTK_CONTAINER(files_window), vbox);

	gtk_widget_show_all(files_window);
	init_keyaccel(files_window);

	gfl->cfl = NULL;

	return gfl;
}

void
open_files (void)
{

	struct gfile_list *gfl = create_files_window("/");

	hx_list_dir(&sessions[0].htlc, "/", 1, 0, gfl);
}

/* fileutils-4.0/lib/human.c */
#define LONGEST_HUMAN_READABLE	32

const char human_suffixes[] = {
	0,	    /* not used */
	'k',	/* kilo */
	'M',	/* Mega */
	'G',	/* Giga */
	'T',	/* Tera */
	'P',	/* Peta */
	'E',	/* Exa */
	'Z',	/* Zetta */
	'Y'	    /* Yotta */
};

/* Convert N to a human readable format in BUF.

   N is expressed in units of FROM_BLOCK_SIZE.  FROM_BLOCK_SIZE must
   be positive.

   If OUTPUT_BLOCK_SIZE is positive, use units of OUTPUT_BLOCK_SIZE in
   the output number.  OUTPUT_BLOCK_SIZE must be a multiple of
   FROM_BLOCK_SIZE or vice versa.

   If OUTPUT_BLOCK_SIZE is negative, use a format like "127k" if
   possible, using powers of -OUTPUT_BLOCK_SIZE; otherwise, use
   ordinary decimal format.  Normally -OUTPUT_BLOCK_SIZE is either
   1000 or 1024; it must be at least 2.  Most people visually process
   strings of 3-4 digits effectively, but longer strings of digits are
   more prone to misinterpretation.  Hence, converting to an
   abbreviated form usually improves readability.  Use a suffix
   indicating which power is being used.  For example, assuming
   -OUTPUT_BLOCK_SIZE is 1024, 8500 would be converted to 8.3k,
   133456345 to 127M, 56990456345 to 53G, and so on.  Numbers smaller
   than -OUTPUT_BLOCK_SIZE aren't modified.  */

char *
human_readable (guint32 n, char *buf,
		int from_block_size, int output_block_size)
{
  guint32 amt;
  uint base;
  int to_block_size;
  uint tenths;
  uint power = 0;
  char *p;

  /* 0 means adjusted N == AMT.TENTHS;
     1 means AMT.TENTHS < adjusted N < AMT.TENTHS + 0.05;
     2 means adjusted N == AMT.TENTHS + 0.05;
     3 means AMT.TENTHS + 0.05 < adjusted N < AMT.TENTHS + 0.1.  */
  uint rounding;

  if (output_block_size < 0)
    {
      base = -output_block_size;
      to_block_size = 1;
    }
  else
    {
      base = 0;
      to_block_size = output_block_size;
    }

  p = buf + LONGEST_HUMAN_READABLE;
  *p = '\0';

  /* Adjust AMT out of FROM_BLOCK_SIZE units and into TO_BLOCK_SIZE units.  */

  if (to_block_size <= from_block_size)
    {
      int multiplier = from_block_size / to_block_size;
      amt = n * multiplier;
      tenths = rounding = 0;

      if (amt / multiplier != n)
	{
	  /* Overflow occurred during multiplication.  We should use
	     multiple precision arithmetic here, but we'll be lazy and
	     resort to floating point.  This can yield answers that
	     are slightly off.  In practice it is quite rare to
	     overflow uintmax_t, so this is good enough for now.  */

	  double damt = n * (double) multiplier;

	  if (! base)
	    sprintf (buf, "%.0f", damt);
	  else
	    {
	      double e = 1;
	      power = 0;

	      do
		{
		  e *= base;
		  power++;
		}
	      while (e * base <= damt && power < sizeof(human_suffixes) - 1);

	      damt /= e;

	      sprintf (buf, "%.1f%c", damt, human_suffixes[power]);
	      if (4 < strlen (buf))
		sprintf (buf, "%.0f%c", damt, human_suffixes[power]);
	    }

	  return buf;
	}
    }
  else
    {
      uint divisor = to_block_size / from_block_size;
      uint r10 = (n % divisor) * 10;
      uint r2 = (r10 % divisor) * 2;
      amt = n / divisor;
      tenths = r10 / divisor;
      rounding = r2 < divisor ? 0 < r2 : 2 + (divisor < r2);
    }


  /* Use power of BASE notation if adjusted AMT is large enough.  */

  if (base && base <= amt)
    {
      power = 0;

      do
	{
	  uint r10 = (amt % base) * 10 + tenths;
	  uint r2 = (r10 % base) * 2 + (rounding >> 1);
	  amt /= base;
	  tenths = r10 / base;
	  rounding = (r2 < base
		      ? 0 < r2 + rounding
		      : 2 + (base < r2 + rounding));
	  power++;
	}
      while (base <= amt && power < sizeof(human_suffixes) - 1);

      *--p = human_suffixes[power];

	  tenths += 2 < rounding + (tenths & 1);

	  if (tenths == 10)
	    {
	      amt++;
	      tenths = 0;
	    }

	      *--p = '0' + tenths;
	      *--p = '.';
	      tenths = 0;
    }

  if (5 < tenths + (2 < rounding + (amt & 1)))
    {
      amt++;

      if (amt == base && power < sizeof(human_suffixes) - 1)
	{
	  *p = human_suffixes[power + 1];
	  *--p = '0';
	  *--p = '.';
	  amt = 1;
	}
    }

  do
    *--p = '0' + (int) (amt % 10);
  while ((amt /= 10) != 0);

  return p;
}

char *human_size(char *sizstr, guint32 size)
{
	return human_readable(size, sizstr, 1, -1024);
}

/* needle must be uppercase :) */
int strcasestr_len (char *haystack, char *needle, size_t len)
{
	char *p, *startn = 0, *np = 0, *end = haystack + len;

	for (p = haystack; p < end; p++) {
		if (np) {
			if (toupper(*p) == *np) {
				if (!*++np)
					return 1;
			} else
			np = 0;
		} else if (toupper(*p) == *needle) {
			np = needle + 1;
			startn = p;
		}
	}
	return 0;
}


static guint16
icon_of_fh (struct hl_filelist_hdr *fh)
{
	guint16 icon;

	if (!memcmp(&fh->ftype, "fldr", 4)) {
		if(	strcasestr_len(fh->fname, "DROP BOX", fh->fnlen) ||
			strcasestr_len(fh->fname, "UPLOAD", fh->fnlen)) {
			icon = ICON_FOLDER_IN;
		}
		else {
			icon = ICON_FOLDER;
		}
	}
	else if (!memcmp(&fh->ftype, "JPEG", 4)
		 || !memcmp(&fh->ftype, "PNGf", 4)
		 || !memcmp(&fh->ftype, "GIFf", 4)
		 || !memcmp(&fh->ftype, "PICT", 4))
		icon = ICON_FILE_IMAGE;
	else if (!memcmp(&fh->ftype, "MPEG", 4)
		 || !memcmp(&fh->ftype, "MPG ", 4)
		 || !memcmp(&fh->ftype, "AVI ", 4)
		 || !memcmp(&fh->ftype, "MooV", 4))
		icon = ICON_FILE_MOOV;
	else if (!memcmp(&fh->ftype, "MP3 ", 4))
		icon = ICON_FILE_NOTE;
	else if (!memcmp(&fh->ftype, "ZIP ", 4))
		icon = ICON_FILE_ZIP;
	else if (!memcmp(&fh->ftype, "SIT", 3))
		icon = ICON_FILE_SIT;
	else if (!memcmp(&fh->ftype, "APPL", 4))
		icon = ICON_FILE_APPL;
	else if (!memcmp(&fh->ftype, "rohd", 4))
		icon = ICON_FILE_DISK;
	else if (!memcmp(&fh->ftype, "HTft", 4))
		icon = ICON_FILE_HTft;
	else if (!memcmp(&fh->ftype, "alis", 4))
		icon = ICON_FILE_alis;
	else
		icon = ICON_FILE;

	return icon;
}

void output_file_list (struct cached_filelist *cfl, struct hl_filelist_hdr *fh,
					   void *data)
{
	GtkWidget *files_list;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	guint16 icon;
	gint row;
	gchar *nulls[2] = {0, 0};
	char humanbuf[LONGEST_HUMAN_READABLE+1], *sizstr;
	char namstr[255];
	struct gfile_list *gfl = (struct gfile_list *)data;
	struct path_hist *path = 0;
	GdkColor col = {0, 0, 0};

	files_list = gfl->hlist;
	gtk_window_set_title(GTK_WINDOW(gfl->window), cfl->path);

	if(strcmp(cfl->path, gfl->path_list->path)) {
		path = g_malloc(sizeof(struct path_hist)+strlen(cfl->path));
		strcpy(path->path, cfl->path);
		path->prev = 0;
		if(gfl->path_list) {
			path->prev = gfl->path_list;
		}
		gfl->path_list = path;
	}

	gtk_widget_set_sensitive(gfl->up_btn, gfl->path_list->prev!=NULL &&
		gtkhx_prefs.file_samewin);

	gtk_hlist_freeze(GTK_HLIST(files_list));
	gtk_hlist_clear(GTK_HLIST(files_list));

	for (fh = cfl->fh; (guint32)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
		 (char *)fh += fh->len + SIZEOF_HL_DATA_HDR) {
		fh->fnlen = ntohl(fh->fnlen);
		fh->len = ntohs(fh->len);
		fh->fsize = ntohl(fh->fsize);
		
		row = gtk_hlist_append(GTK_HLIST(files_list), nulls);
		gtk_hlist_set_row_data(GTK_HLIST(files_list), row, fh);
		icon = icon_of_fh(fh);
		load_icon(files_list, icon, &icon_files, 1, &pixmap, &mask);
		
		if (fh->fnlen > 255)
			fh->fnlen = 255;
		memcpy(namstr, fh->fname, fh->fnlen);
		namstr[fh->fnlen] = 0;
		if (!memcmp(&fh->ftype, "fldr", 4)) {
			sizstr = humanbuf;
			sprintf(sizstr, "(%u)", (fh->fsize));
		}
		else {
			sizstr = human_size(humanbuf, fh->fsize);
		}
		gtk_hlist_set_foreground(GTK_HLIST(files_list), row, &col);
		gtk_hlist_set_text(GTK_HLIST(files_list), row, 0, sizstr);
		
		if (!pixmap) {
			gtk_hlist_set_text(GTK_HLIST(files_list), row, 1, namstr);
		}
		else {
			gtk_hlist_set_pixtext(GTK_HLIST(files_list), row, 1, namstr, 34,
								  pixmap, mask);
		}
	}
	gtk_hlist_thaw(GTK_HLIST(files_list));
	gtk_hlist_select_row(GTK_HLIST(files_list), (gfl->row-1)?(gfl->row-1):gfl->row, 0);
	gtk_hlist_moveto(GTK_HLIST(files_list), (gfl->row-1)?(gfl->row-1):gfl->row, 0, .5, 0);
	

	gfl->in_use = 0;
}

void set_name_comment(GtkWidget *btn, gpointer data)
{
	GtkWidget *name_entry = gtk_object_get_data(GTK_OBJECT(btn), "name");
	GtkWidget *comments_text = gtk_object_get_data(GTK_OBJECT(btn), "comments");
	char *path = gtk_object_get_data(GTK_OBJECT(btn), "path");
	char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
	char *comments = gtk_editable_get_chars(GTK_EDITABLE(comments_text), 0, -1);
	char *file;

	file = dirchar_basename(path); 
	task_new(&sessions[0].htlc, 0, 0, 0, "set file info");
	if (file != path) {
		guint16 hldirlen = 0;
		guint8 *hldir = path_to_hldir(path, &hldirlen, 1);
		hlwrite(&sessions[0].htlc, HTLC_HDR_FILE_SETINFO, 0, 4,
				HTLC_DATA_FILE_NAME, strlen(file), file,
				HTLC_DATA_FILE_RENAME, strlen(name), name,
				HTLC_DATA_FILE_COMMENT, strlen(comments), comments,
				HTLC_DATA_DIR, hldirlen, hldir); 
		g_free(hldir);
	} else {
		hlwrite(&sessions[0].htlc, HTLC_HDR_FILE_SETINFO, 0, 3,
				HTLC_DATA_FILE_NAME, strlen(file), file,
				HTLC_DATA_FILE_RENAME, strlen(name), name,
				HTLC_DATA_FILE_COMMENT, strlen(comments), comments); 
	} 
	
	g_free(comments);
}

void close_file_info(GtkWidget *win, char *path)
{
	g_free(path);
}

void output_file_info(char *path, char *name, char *creator, char *type, 
					  char *comments, char *modified, char *created, 
					  guint32 size)
{
	GtkWidget *window;
	GtkWidget *creator_label;
	GtkWidget *type_label;
	GtkWidget *modified_label;
	GtkWidget *created_label;
	GtkWidget *size_label;
	GtkWidget *name_entry;
	GtkWidget *name_label;
	GtkWidget *comments_text;
	GtkWidget *name_hbox;
	GtkWidget *vbox;
	GtkWidget *savebtn;
	char *text;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_border_width (GTK_CONTAINER(window), 5);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
					   (GtkSignalFunc) gtk_widget_destroy, window);
	gtk_window_set_title(GTK_WINDOW(window), _("File Info"));

	name_entry = gtk_entry_new();
	comments_text = gtk_text_new(0,0);
	vbox = gtk_vbox_new(0,0);
	name_hbox = gtk_hbox_new(0,0);
	savebtn = gtk_button_new_with_label(_("Save"));

	text = g_strdup_printf("%s: %s", _("Creator"), creator);
	creator_label = gtk_label_new(text);
	text = g_strdup_printf("%s: %s", _("Type"), type);
	type_label = gtk_label_new(text);
	text = g_strdup_printf("%s: %s", _("Modified"), modified);
	modified_label = gtk_label_new(text);
	text = g_strdup_printf("%s: %s", _("Created"), created);
	created_label = gtk_label_new(text);
	text = g_strdup_printf("%s: %u", _("Size"), size);
	size_label = gtk_label_new(text);
	name_label = gtk_label_new(_("Name: "));
	gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_text_insert(GTK_TEXT(comments_text), 0, 0, 0, comments,
					strlen(comments));
	gtk_text_set_editable(GTK_TEXT(comments_text), TRUE);
	g_free(text);

	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), name_hbox, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(name_hbox), name_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(name_hbox), name_entry, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), creator_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), type_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), size_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), created_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), modified_label, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), comments_text, 0, 0, 4);
	gtk_box_pack_start(GTK_BOX(vbox), savebtn, 0, 0, 0);

	gtk_object_set_data(GTK_OBJECT(savebtn), "name", name_entry);
	gtk_object_set_data(GTK_OBJECT(savebtn), "comments", comments_text);
	gtk_object_set_data(GTK_OBJECT(savebtn), "path", path);
	gtk_signal_connect(GTK_OBJECT(savebtn), "clicked", 
					   GTK_SIGNAL_FUNC(set_name_comment), NULL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(close_file_info), path);

	gtk_widget_show_all(window);
	init_keyaccel(window);
}

struct cached_filelist *cfl_lookup (const char *path)
{
	struct gfile_list *gfl;

	gfl = gfl_with_path((char *)path);
	if(!gfl) {
		struct cached_filelist *cfl = g_malloc0(sizeof(struct cached_filelist));
		return cfl;
	}
	else {
		return gfl->cfl;
	}
}

void cfl_print (struct cached_filelist *cfl, void *data)
{
	struct hl_filelist_hdr *fh = cfl->fh;

	if(data) {
		hx_output.file_list(cfl, fh, data);
	}
}

struct x_fhdr {
	guint16 enc PACKED;
	guint8 len, name[1] PACKED;
};

guint8 *path_to_hldir (const char *path, guint16 *hldirlen, int is_file)
{
	guint8 *hldir;
	struct x_fhdr *fh;
	char const *p, *p2;
	guint16 pos = 2, dc = 0;
	guint8 nlen;

	hldir = g_malloc(2);
	p = path;
	while ((p2 = strchr(p, dir_char))) {
		if (!(p2 - p)) {
			p++;
			continue;
		}
		nlen = (guint8)(p2 - p);
		pos += 3 + nlen;
		hldir = g_realloc(hldir, pos);
		fh = (struct x_fhdr *)(&(hldir[pos - (3 + nlen)]));
		memset(&fh->enc, 0, 2);
		fh->len = nlen;
		memcpy(fh->name, p, nlen);
		dc++;
		p = p2 + 1;
	}
	if (!is_file && *p) {
		nlen = (guint8)strlen(p);
		pos += 3 + nlen;
		hldir = g_realloc(hldir, pos);
		fh = (struct x_fhdr *)(&(hldir[pos - (3 + nlen)]));
		memset(&fh->enc, 0, 2);
		fh->len = nlen;
		memcpy(fh->name, p, nlen);
		dc++;
	}
	*((guint16 *)hldir) = htons(dc);

	*hldirlen = pos;
	return hldir;
}

char *dirchar_basename (char *path)
{
	size_t len;

	len = strlen(path);
	while (len--) {
		if (path[len] == dir_char)
			return path+len+1;
	}

	return path;
}

inline void dirchar_fix (char *lpath)
{
	char *p;

	for (p = lpath; *p; p++)
		if (*p == '/')
			*p = (dir_char == '/' ? ':' : dir_char);
}

inline void dirmask (char *dst, char *src, char *mask)
{
	while (*mask && *src && *mask++ == *src++) ;

	strcpy(dst, src);
}

int exists_remote (char *path)
{
	struct gfile_list *gfl;
	struct cached_filelist *cfl = NULL;
	struct hl_filelist_hdr *fh;
	char *p, *ent, buf[MAXPATHLEN];
	int blen = 0, len;

	len = strlen(path);
	if (*path != dir_char) {
		snprintf(buf, MAXPATHLEN, "%s%c%.*s", "/",
				 dir_char, len, path);
		len = strlen(buf); /* Unfortunately we can't trust snprintf
							  return value .. */
		path = buf;
	}
	ent = path;
	for (p = path + len - 1; p >= path; p--)
		if (*p == dir_char) {
			ent = p+1;
			while (p > path && *p == dir_char)
				p--;
			blen = (p+1) - path;
			len -= ent - path;
			break;
		}
	if (!*ent)
		return -1;


	for(gfl = gfile_list; gfl; gfl = gfl->prev) {
		if(!strncmp(gfl->cfl->path, path, blen)) {
			cfl = gfl->cfl;
			break;
		}
	}

	if (!cfl) {
		guint16 hldirlen;
		guint8 *hldir;

		snprintf(buf, MAXPATHLEN, "%.*s", blen, path);
		path = buf; 
		len = strlen(path);
		while (len > 1 && path[--len] == dir_char)
			path[len] = 0;
		cfl = g_malloc0(sizeof(struct cached_filelist));
		cfl->completing = COMPLETE_EXPAND;
		cfl->path = g_strdup(path);
		hldir = path_to_hldir(path, &hldirlen, 0);
		task_new(&sessions[0].htlc, rcv_task_file_list, cfl, 0, "ls_exists");
		hlwrite(&sessions[0].htlc, HTLC_HDR_FILE_LIST, 0, 1,
			HTLC_DATA_DIR, hldirlen, hldir);
		g_free(hldir);
		return 0;
	}
	if (!cfl->fh)
		return 0;

	for (fh = cfl->fh; (guint32)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
		 (char *)fh += fh->len + SIZEOF_HL_DATA_HDR) {
		if ((int)fh->fnlen == len && !strncmp(fh->fname, ent, len))
			return 1;
	}

	return 0;
}

void hx_list_dir (struct htlc_conn *htlc, const char *path, int reload,
				  int recurs, void *data)
{
	guint16 hldirlen;
	guint8 *hldir;
	struct cached_filelist *cfl;
	struct gfile_list *gfl = data;

	if(gfl->cfl && (strcmp(gfl->cfl->path, path) && !gtkhx_prefs.file_samewin)) {
		gfl = create_files_window((char *)path);
	}

	if(gfl->cfl) {
		g_free(gfl->cfl);
	}

	gfl->cfl = g_malloc0(sizeof(struct cached_filelist));
	cfl = gfl->cfl;
	cfl->path = g_strdup(path);
	gfl->in_use = 1;
	if(recurs)
		cfl->completing = COMPLETE_LS_R;

	hldir = path_to_hldir(path, &hldirlen, 0);
	task_new(htlc, rcv_task_file_list, cfl, (void *)gfl, "ls");
	hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
		HTLC_DATA_DIR, hldirlen, hldir);
	g_free(hldir);
}

void hx_make_dir(struct htlc_conn *htlc, char *path)
{
	guint16 hldirlen;
	guint8 *hldir;


	hldir = path_to_hldir(path, &hldirlen, 0);
	task_new(htlc, 0, 0, 0, "mkdir");
	hlwrite(htlc, HTLC_HDR_FILE_MKDIR, 0, 1, HTLC_DATA_DIR, hldirlen, hldir);


	g_free(hldir);
}

void hx_file_delete (struct htlc_conn *htlc, char *path)
{
	guint16 hldirlen;
	guint8 *hldir;
	char *file;

	task_new(htlc, 0, 0, 0, "rm");
	file = dirchar_basename(path); 
	if (file != path) {
		hldir = path_to_hldir(path, &hldirlen, 1);
		hlwrite(htlc, HTLC_HDR_FILE_DELETE, 0, 2,
			HTLC_DATA_FILE_NAME, strlen(file), file,
			HTLC_DATA_DIR, hldirlen, hldir);
		g_free(hldir);
	} else {
		hlwrite(htlc, HTLC_HDR_FILE_DELETE, 0, 1,
			HTLC_DATA_FILE_NAME, strlen(file), file);
	}
}
void hx_file_info(struct htlc_conn *htlc, char *_path)
{
	char *file;
	guint8 *hldir;
	guint16 hldirlen;
	char *path = g_strdup(_path);

	task_new(htlc, rcv_task_file_getinfo, path, 0, "finfo");
	file = dirchar_basename(path); 

	if(file != path) {
		hldir = path_to_hldir(path, &hldirlen, 1);
		hlwrite(htlc, HTLC_HDR_FILE_GETINFO, 0, 2,
				HTLC_DATA_FILE_NAME, strlen(file), file,
				HTLC_DATA_DIR, hldirlen, hldir);
		g_free(hldir);
	}
	else {
		hlwrite(htlc, HTLC_HDR_FILE_GETINFO, 0, 1, HTLC_DATA_FILE_NAME,
				strlen(file), file);
	}
}

void hx_put_file(struct htlc_conn *htlc, char *lpath, char *rpath)
{
	struct htxf_conn *htxf;

	htxf = xfer_new(lpath, rpath, XFER_PUT);
	htxf->filter_argv = 0;
	htxf->opt.retry = 0;
}

void hx_file_link (struct htlc_conn *htlc, char *src_path, char *dst_path)
{
	char *src_file, *dst_file;
	guint16 hldirlen, rnhldirlen;
	guint8 *hldir, *rnhldir;

	src_file = dirchar_basename(src_path);
	dst_file = dirchar_basename(dst_path);
	hldir = path_to_hldir(src_path, &hldirlen, 1);
	rnhldir = path_to_hldir(dst_path, &rnhldirlen, 1);
	task_new(htlc, 0, 0, 0, "ln");
	hlwrite(htlc, HTLC_HDR_FILE_SYMLINK, 0, 4,
		HTLC_DATA_FILE_NAME, strlen(src_file), src_file,
		HTLC_DATA_DIR, hldirlen, hldir,
		HTLC_DATA_DIR_RENAME, rnhldirlen, rnhldir,
		HTLC_DATA_FILE_RENAME, strlen(dst_file), dst_file);
	g_free(rnhldir);
	g_free(hldir);
}

void hx_file_move (struct htlc_conn *htlc, char *src_path, char *dst_path)
{
	char *dst_file, *src_file;
	guint16 hldirlen, rnhldirlen;
	guint8 *hldir, *rnhldir;
	size_t len;

	dst_file = dirchar_basename(dst_path);
	src_file = dirchar_basename(src_path);

	hldir = path_to_hldir(src_path, &hldirlen, 1);
	len = strlen(dst_path) - (strlen(dst_path) - (dst_file - dst_path));
	if (len && (len != strlen(src_path) - (strlen(src_path) - (src_file -
															   src_path)) ||
				memcmp(dst_path, src_path, len))) {
		rnhldir = path_to_hldir(dst_path, &rnhldirlen, 1);
		task_new(htlc, 0, 0, 0, "mv");
		hlwrite(htlc, HTLC_HDR_FILE_MOVE, 0, 3,
			HTLC_DATA_FILE_NAME, strlen(src_file), src_file,
			HTLC_DATA_DIR, hldirlen, hldir,
			HTLC_DATA_DIR_RENAME, rnhldirlen, rnhldir);
		g_free(rnhldir);
	}
	if (*dst_file && strcmp(src_file, dst_file)) {
		task_new(htlc, 0, 0, 0, "mv");
		hlwrite(htlc, HTLC_HDR_FILE_SETINFO, 0, 3,
			HTLC_DATA_FILE_NAME, strlen(src_file), src_file,
			HTLC_DATA_FILE_RENAME, strlen(dst_file), dst_file,
		HTLC_DATA_DIR, hldirlen, hldir);
	}
	g_free(hldir);
}

