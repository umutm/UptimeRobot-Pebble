#include <pebble.h>
#include "pebble-assist.h"
#include "common.h"
#include "monitorlist.h"
#include "details.h"

#define MAX_MONITORS 50

static Monitor monitors[MAX_MONITORS];

static int num_monitors;
static char monitorError[128];
static char monitorId[128];

static void monitors_clean_list();
static uint16_t monitors_menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *callback_context);
static uint16_t monitors_menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static int16_t monitors_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static int16_t monitors_menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void monitors_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context);
static void monitors_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static void monitors_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void monitors_menu_select_long_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);

static Window *monitors_window;
static MenuLayer *monitors_menu_layer;

static GBitmap *x_menu_icon;
static GBitmap *check_menu_icon;
static GBitmap *pause_menu_icon;
static GBitmap *question_menu_icon;

void monitorslist_init(void) {
	monitors_window = window_create();

	x_menu_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_X_ICON_SMALL);
	check_menu_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHECK_ICON_SMALL);
	pause_menu_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PAUSE_ICON_SMALL);
	question_menu_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_QUESTION_ICON_SMALL);

	monitors_menu_layer = menu_layer_create_fullscreen(monitors_window);
	menu_layer_set_callbacks(monitors_menu_layer, NULL, (MenuLayerCallbacks) {
		.get_num_sections = monitors_menu_get_num_sections_callback,
		.get_num_rows = monitors_menu_get_num_rows_callback,
		.get_header_height = monitors_menu_get_header_height_callback,
		.get_cell_height = monitors_menu_get_cell_height_callback,
		.draw_header = monitors_menu_draw_header_callback,
		.draw_row = monitors_menu_draw_row_callback,
		.select_click = monitors_menu_select_callback,
		.select_long_click = monitors_menu_select_long_callback,
	});

	menu_layer_set_click_config_onto_window(monitors_menu_layer, monitors_window);
	menu_layer_add_to_window(monitors_menu_layer, monitors_window);
}

void monitorslist_show() {
	monitors_clean_list();
	window_stack_push(monitors_window, true);
}

void monitorslist_destroy(void) {
	gbitmap_destroy(check_menu_icon);
	gbitmap_destroy(question_menu_icon);
	gbitmap_destroy(x_menu_icon);
	gbitmap_destroy(pause_menu_icon);
	layer_remove_from_parent(menu_layer_get_layer(monitors_menu_layer));
	menu_layer_destroy_safe(monitors_menu_layer);
	window_destroy_safe(monitors_window);
}

static void monitors_clean_list() {
	memset(monitors, 0x0, sizeof(monitors));
	num_monitors = 0;
	monitorError[0] = '\0';
	menu_layer_set_selected_index(monitors_menu_layer, (MenuIndex) { .row = 0, .section = 0 }, MenuRowAlignBottom, false);
	menu_layer_reload_data_and_mark_dirty(monitors_menu_layer);
}

bool monitorslist_is_on_top() {
	return monitors_window == window_stack_get_top_window();
}

void monitorslist_in_received_handler(DictionaryIterator *iter) {
	Tuple *index_tuple = dict_find(iter, MONITOR_INDEX);
	Tuple *id_tuple = dict_find(iter, MONITOR_ID);
	Tuple *name_tuple = dict_find(iter, MONITOR_NAME);
	Tuple *url_tuple = dict_find(iter, MONITOR_URL);
	Tuple *status_tuple = dict_find(iter, MONITOR_STATUS);

	if (index_tuple && name_tuple) {
		Monitor monitor;
		monitor.index = index_tuple->value->int16;
		strncpy(monitor.id, id_tuple->value->cstring, sizeof(monitor.id));
		strncpy(monitor.name, name_tuple->value->cstring, sizeof(monitor.name));
		strncpy(monitor.url, url_tuple->value->cstring, sizeof(monitor.url));
		monitor.status = status_tuple->value->int16;
		monitors[monitor.index] = monitor;
		num_monitors++;
		APP_LOG(APP_LOG_LEVEL_DEBUG, "%s - %s", monitor.id, monitor.name);
		menu_layer_reload_data_and_mark_dirty(monitors_menu_layer);
	}
}

static uint16_t monitors_menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *callback_context) {
    return 1;
}

static uint16_t monitors_menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    return (num_monitors) ? num_monitors : 1;
}

static int16_t monitors_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static int16_t monitors_menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    return MENU_CELL_BASIC_CELL_HEIGHT;
}

static void monitors_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context) {
    menu_cell_basic_header_draw(ctx, cell_layer, "Monitors");
}

static void monitors_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
    if (strlen(monitorError) != 0) {
        menu_cell_basic_draw(ctx, cell_layer, "Error!", monitorError, NULL);
    } else if (num_monitors == 0) {
        menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
    } else {
		GBitmap *status_image;
		switch (monitors[cell_index->row].status) {
			case 0:
				status_image = pause_menu_icon;
				break;
			case 1:
				status_image = question_menu_icon;
				break;
			case 2:
				status_image = check_menu_icon;
				break;
			case 8:
				status_image = x_menu_icon;
				break;
			case 9:
				status_image = x_menu_icon;
				break;
			default:
				status_image = NULL;
		}
        menu_cell_basic_draw(ctx, cell_layer, monitors[cell_index->row].name, monitors[cell_index->row].url, status_image);
    }
}

static void monitors_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    strncpy(monitorId, monitors[cell_index->row].id, sizeof(monitorId));
	APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", monitorId);
	getDetails(monitorId);
}

static void monitors_menu_select_long_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    vibes_double_pulse();

}
