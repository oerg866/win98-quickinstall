#include "qi_assert.h"
#include "install_ui.h"
#include "util.h"
#include "version.h"

#include <dialog.h>
#include <stdio.h>

char ui_BackTitle[] = LUNMERCY_BACKTITLE;
char ui_EmptyString[] = "";
char ui_ButtonLabelNext[] = "Next";
char ui_ButtonLabelOK[] = "OK";
char ui_ButtonLabelYes[] = "Yes";
char ui_ButtonLabelNo[] = "No";
char ui_ButtonLabelCancel[] = "Cancel";
char ui_ButtonLabelBack[] = "Back";
char ui_ButtonLabelFinished[] = "< FINISHED >";
char ui_ButtonLabelExitToShell[] = "Exit To Shell";
char ui_ButtonLabelReboot[] = "Reboot";
char ui_YesNoEscapeHint[] = "QuickInstall [ESC: Cancel]";

char* ui_makeDialogMenuLabel(char *fmt, ...) {
    char *label = calloc(1, UI_DIALOG_LABEL_LENGTH);
    va_list args;
    va_start(args, fmt);
    vsnprintf(label, UI_DIALOG_LABEL_LENGTH, fmt, args);
    va_end(args);
    return label;
}

inline char** ui_allocateDialogMenuLabelList(int count) {
    char **ret = calloc(1, (count + 1) * 2 * sizeof(char*));

    ret[count * 2 + 0] = NULL;  // End Of List Marker
    ret[count * 2 + 1] = NULL;

    return ret;
}

char** ui_makeDialogMenuLabelList(int count, ...) {
    char **menuLabelList = ui_allocateDialogMenuLabelList(count);
    va_list args;

    va_start(args, count);

    for (int i = 0; i < count; i++) {
        menuLabelList[i] = va_arg(args, char*);
    }

    va_end(args);
    return menuLabelList;
}

void ui_destroyDialogMenuLabelList(char **list) {
    char **currentEntry = list;
    while (currentEntry[0] != NULL && currentEntry[1] != NULL) {
        free(currentEntry[0]);
        free(currentEntry[1]);
        currentEntry += 2;
    }
    free(list);
}

int ui_getMenuLabelListItemCount(char **list) {
    int count = 0;
    while (!(list[0] == NULL && list[1] == NULL)) {
        count++;
        list += 2;
    }
    return count;
}

void ui_addDialogMenuLabelToList(char ***ptrToList, char *label, char *description) {
    int listSize = ui_getMenuLabelListItemCount(*ptrToList);
    *ptrToList = realloc(*ptrToList, (listSize + 1 + 1) * 2 * sizeof(char*));
    ui_setMenuLabelListEntry(*ptrToList, listSize, label, description);
    ui_setMenuLabelListEntry(*ptrToList, listSize+1, NULL, NULL);    
}

static int ui_getMenuLabelIndexFromString(char **list, const char *toFind) {
    int index = 0;
    while (list[0] != NULL && list[1] != NULL) {
        if (util_stringEquals(list[0], toFind))
            return index;
        index++;
        list += 2;
    }
    return UI_MENU_ERROR;
}


int ui_showTextBox(const char *title, const char *fileName) {
    UI_PREPARE_DIALOG();
    dialog_vars.exit_label = ui_ButtonLabelNext;
    return dialog_textbox(title, fileName, UI_TEXTBOX_HEIGHT, UI_TEXTBOX_WIDTH);
}

int ui_showMenu(const char *prompt, char **menuItems, bool showBackButton) {
    int ret = 0;
    bool oldNoCancelVar = dialog_vars.nocancel;

    UI_PREPARE_DIALOG();
    
    dlg_clr_result();

    dialog_vars.ok_label = ui_ButtonLabelNext;
    dialog_vars.cancel_label = ui_ButtonLabelBack;   
    dialog_vars.nocancel = !showBackButton;
    
    ret = dialog_menu(NULL, prompt, 0, 0, 0, ui_getMenuLabelListItemCount(menuItems), menuItems);

    // Reset nocancel so later UI functions don't get into trouble
    dialog_vars.nocancel = oldNoCancelVar;

    if (ret == 0) {
        ret = ui_getMenuLabelIndexFromString(menuItems, dialog_vars.input_result);
    } else {
        ret = UI_MENU_CANCELED;
    }

    return ret;
}

char *ui_getMenuResultString() {
    if (dialog_vars.input_result) {
        return dialog_vars.input_result;
    } else {
        return ui_EmptyString;  // to make sure we don't crash by using a NULL string later...
    }
}

int ui_showYesNoCustomLabels(char *yesLabel, char *noLabel, const char *prompt, bool cancelable) {
    UI_PREPARE_DIALOG();
    dialog_vars.yes_label = yesLabel;
    dialog_vars.no_label = noLabel;
    
    while (true) {

        // If it's cancelable we return the result as-is. If not, we wait until the result is valid.
        int result = dialog_yesno(cancelable ? ui_YesNoEscapeHint : NULL, prompt, 0, 0);

        if (cancelable || result != UI_YESNO_CANCELED) {
            return result;
        }
    }
}

void ui_showMessageBox(const char *message) {
    UI_PREPARE_DIALOG();
    dialog_vars.ok_label = ui_ButtonLabelOK;
    dialog_msgbox(NULL, message, 0, 0, 1);
}

int ui_runCommand(const char *message, const char *command) {
    FILE *fd = dlg_popen(command, "r");
    UI_PREPARE_DIALOG();
    dlg_progressbox(NULL, message, UI_COMMAND_HEIGHT, UI_COMMAND_WIDTH, 0, fd);
    return WEXITSTATUS(pclose(fd));
}

void ui_showInfoBox(const char *message) {
    UI_PREPARE_DIALOG();
    dialog_msgbox(NULL, message, 0, 0, 0);
}

/* Gauges are extremely broken in libdialog, so we don't use them and instead do things ourselves. GOD DAMN IT.
static void *ui_StaticGauge = NULL;
static int ui_Progress = 0; */

/* -4 for the spacing and border  2 for the edges of the actual progress bar*/
#define UI_PROGRESS_BAR_TOTALWIDTH (UI_TEXTBOX_WIDTH - 4)
#define UI_PROGRESS_BAR_WIDTH (UI_TEXTBOX_WIDTH - 4 - 3 - 1)
#define UI_PROGRESS_VALUE(progress, maximum) ((int) (((double) progress) * (double) UI_PROGRESS_BAR_WIDTH / (double) maximum))
#define UI_PROGRESS_BAR_SET_TEXT_COLOR() printf("\033[30;47m");
#define UI_SET_POSITION(x,y) { printf("\033[%d;%dH", (y), (x)); fflush (stdout); }

/* No idea if it's possible to get this from libdialog so for now we hardcode it... */
#define UI_PROGRESS_BAR_START_X (7)
#define UI_PROGRESS_BAR_START_Y (12)

static int ui_Progress = 0;

void ui_progressBoxInit(const char *title) {
    UI_PREPARE_DIALOG();
/*  ui_StaticGauge = dlg_reallocate_gauge(ui_StaticGauge, NULL, title, UI_GAUGEBOX_HEIGHT, UI_GAUGEBOX_WIDTH, 0); */

    dialog_msgbox(title, NULL, 3, UI_PROGRESS_BAR_TOTALWIDTH, 0);
    ui_Progress = 0;
}

void ui_progressBoxUpdate(int progress, int maximum) {
//    printf("progress %lu maximum %lu\r\n", progress, maximum);
    progress = UI_PROGRESS_VALUE(progress, maximum);
    if (ui_Progress != progress) {
        //printf ("ui_Progress: %d, progress %d, count %d\n",ui_Progress, progress, progress - ui_Progress);
        UI_SET_POSITION(UI_PROGRESS_BAR_START_X + ui_Progress, UI_PROGRESS_BAR_START_Y);

        while(ui_Progress != progress) {
            putchar(' ');
            ui_Progress++;
        }

        fflush(stdout);
    }
}

void ui_progressBoxDeinit() {
    /*  There seems to be a bug when compiling libdialog for i486 targets, causing a segfault.
    This does not happen on regular x64 targets so .... no clue. So we just reallocate when we
    want to use another gauge.
    
    dlg_free_gauge(ui_StaticGauge); */
}

void ui_init() {
    init_dialog(stdin, stdout);
    clear();
    dlg_clear();
    dialog_vars.backtitle = ui_BackTitle;
}

void ui_deinit() {
    end_dialog();
}
