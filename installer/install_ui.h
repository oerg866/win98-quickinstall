#ifndef INSTALL_UI_H
#define INSTALL_UI_H

/*
 * LUNMERCY - Installer UI component 
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdint.h>
#include <stdbool.h>

#define UI_PREPARE_DIALOG() { clear(); dlg_clear(); dlg_put_backtitle(); }
#define UI_DIALOG_LABEL_LENGTH  1024

#define UI_TEXTBOX_WIDTH         74
#define UI_TEXTBOX_HEIGHT        20

#define UI_COMMAND_WIDTH         74
#define UI_COMMAND_HEIGHT        15

#define UI_GAUGEBOX_WIDTH        74
#define UI_GAUGEBOX_HEIGHT       7

#define UI_MENU_CANCELED         -1
#define UI_MENU_ERROR            -2

/* Allocates and creates a format string for a label for dialog menu purposes. */
char* ui_makeDialogMenuLabel(char *fmt, ...);
/* Allocates a char* pointer array for labels and description 
   plus one extra for the FINISHED label
   plus one extra for the (so to fit (count + 2) * 2 strings) */
char** ui_allocateDialogMenuLabelList(int count);
/* Allocats a list of menu labels + descriptions (so count * 2 strings) and initializes them with given strings */
char** ui_makeDialogMenuLabelList(int count, ...);
/* Frees a MenuLabelList and all its entries at once */
void ui_destroyDialogMenuLabelList(char **list);
/* Sets a label/description pair in a MenuLabelList at given index. 
    the strings MUST be allocated specifically for this and free-able! 
    ui_makeDialogMenuLabel may be used for this. */
#define ui_setMenuLabelListEntry(list, index, label, description) { (list)[(index)*2+0] = label; (list)[(index)*2+1] = description; }
/* Gets a pointer to label/description pair in a MenuLabelList at given index. */
#define ui_getMenuLabelListEntry(list, index) (&(list)[(index)*2])
/* Get amount of entries in a MenuLabelList */
int ui_getMenuLabelListItemCount(char **list);
/* Reallocates and adds a new entry to a MenuLabelList. This is slow as hell so use sparsly. */
void ui_addDialogMenuLabelToList(char ***ptrToList, char *label, char *description);

/* Shows text box. The exitLabel parameter is the text for the button. */
int ui_showTextBox(const char *title, const char *fileName);
/* Shows a menu. Accepts a Dialog Menu Label List created by makeDialogMenuLabelList as parameter. 
   Contrary to dialog's API, this returns the INDEX of the option that was selected.*/
int ui_showMenu(const char *prompt, char **menuItems, bool showBackButton);
/* Get the result string of the menu.*/
char *ui_getMenuResultString();
/* Shows a "Yes / No" dialog box with custom labels for the yes and no buttons. */
int ui_showYesNoCustomLabels(char *yesLabel, char *noLabel, const char *prompt);
/* Shows a message box. */
void ui_showMessageBox(const char *message);
/* Runs a command in a progress box, returns the command's exit code */
int ui_runCommand(const char *message, const char *command);
/* Shows an info box without any buttons */
void ui_showInfoBox(const char *message);
/* Initializes a progress bar window. Don't forget to call ui_progressBoxDeinit !! */
void ui_progressBoxInit(const char *title);
/* Sets the progress bar's progress value */
void ui_progressBoxUpdate(int progress, int maximum);
/* Deinitializes the progress bar window. */
void ui_progressBoxDeinit();

/* Init UI / libdialog. Must be called once at the start */
void ui_init();
/* Deinit UI / libdialog. Must be calld once at the end */
void ui_deinit();

/* TODO make this nicer with a static lut in install_ui.c and an enum as parameter for the functions */
extern char ui_BackTitle[];
extern char ui_EmptyString[];
extern char ui_ButtonLabelNext[];
extern char ui_ButtonLabelOK[];
extern char ui_ButtonLabelYes[];
extern char ui_ButtonLabelNo[];
extern char ui_ButtonLabelCancel[];
extern char ui_ButtonLabelBack[];
extern char ui_ButtonLabelFinished[];
extern char ui_ButtonLabelExitToShell[];
extern char ui_ButtonLabelReboot[];

#endif