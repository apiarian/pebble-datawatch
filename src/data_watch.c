#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *date_layer;

static void update_display() {
	time_t now = time(NULL);
	const struct tm *t = localtime(&now);

	static char time_text[] = "00:00";
	static char date_text[] = "wkd YYYY-MM-DD.....";

	//strftime(time_text, sizeof(time_text), "%R", t);
	clock_copy_time_string(time_text, sizeof(time_text));
	text_layer_set_text(time_layer,time_text);

	strftime(date_text, sizeof(date_text), "%a-%F", t);
	text_layer_set_text(date_layer,date_text);
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits unit_changed) {
	update_display();
}

static void init(void) {
	GFont time_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

	window = window_create();
	const bool animated = true;
	window_stack_push(window, animated);

	window_set_background_color(window,GColorWhite);
	Layer *root_layer = window_get_root_layer(window);
	GRect frame = layer_get_frame(root_layer);

	int layer_height = 32;
	int layer_accumulator = layer_height;
	time_layer = text_layer_create(GRect(0,0,frame.size.w, layer_height));
	text_layer_set_background_color(time_layer,GColorBlack);
	text_layer_set_text_color(time_layer,GColorWhite);
	text_layer_set_font(time_layer,time_font);
	text_layer_set_text_alignment(time_layer,GTextAlignmentCenter);
	layer_add_child(root_layer,text_layer_get_layer(time_layer));

	layer_height = 28;
	date_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w,layer_height));
	text_layer_set_background_color(date_layer,GColorBlack);
	text_layer_set_text_color(date_layer,GColorWhite);
	text_layer_set_font(date_layer,font);
	text_layer_set_text_alignment(date_layer,GTextAlignmentCenter);
	layer_add_child(root_layer,text_layer_get_layer(date_layer));
	layer_accumulator += layer_height;

	update_display();

	tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
}

static void deinit(void) {
	window_destroy(window);
	text_layer_destroy(time_layer);
	text_layer_destroy(date_layer);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
}
