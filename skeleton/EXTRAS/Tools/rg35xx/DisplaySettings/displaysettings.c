#include "msettings.h"
#include "common/api.h"
#include <SDL.h>

static int on_contrast_change(struct MenuList* list, int i) {
    SetContrast(list->items[i].value);
    return MENU_CALLBACK_NOP;
}
static int on_colortemp_change(struct MenuList* list, int i) {
    SetColortemp(list->items[i].value);
    return MENU_CALLBACK_NOP;
}
static int on_saturation_change(struct MenuList* list, int i) {
    SetSaturation(list->items[i].value);
    return MENU_CALLBACK_NOP;
}

static char* contrast_labels[] = { "0","1","2","3","4","5","6","7","8","9","10", NULL };
static char* colortemp_labels[] = { "0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20", NULL };
static char* saturation_labels[] = { "0","1","2","3","4","5","6","7","8","9","10", NULL };

static struct MenuItem menu_items[] = {
    { "Contrast", "Adjust display contrast", contrast_labels, NULL, 0, 5, NULL, NULL, on_contrast_change },
    { "Color Temp", "Adjust color temperature", colortemp_labels, NULL, 0, 10, NULL, NULL, on_colortemp_change },
    { "Saturation", "Adjust display saturation", saturation_labels, NULL, 0, 5, NULL, NULL, on_saturation_change },
    { NULL }
};

static struct MenuList menu = {
    .type = MENU_LIST,
    .items = menu_items,
};

int main(int argc, char** argv) {
    PAD_init();
    InitSettings();
    Menu_options(&menu)m
    QuitSettings();
    PAD_quit();
    return 0;
}