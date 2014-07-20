#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *battery_layer;
static TextLayer *bluetooth_layer;

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
static void update_battery(BatteryChargeState charge_state) {
	static char battery_text[] = "bat: 100%";
	if(charge_state.is_charging) {
		snprintf(battery_text, sizeof(battery_text), "bat: chrg");
	} else {
		snprintf(battery_text, sizeof(battery_text), "bat: %d%%", charge_state.charge_percent);
	}
	text_layer_set_text(battery_layer, battery_text);
}
static void update_bluetooth(bool connected) {
	text_layer_set_text(bluetooth_layer, connected ? "blu: yes" : "blu: no");
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits unit_changed) {
	update_display();
	update_battery(battery_state_service_peek());
	update_bluetooth(bluetooth_connection_service_peek());
}

static void init(void) {
	GFont time_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	GFont mid_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

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
	text_layer_set_font(date_layer, mid_font);
	text_layer_set_text_alignment(date_layer,GTextAlignmentCenter);
	layer_add_child(root_layer,text_layer_get_layer(date_layer));
	layer_accumulator += layer_height;

	layer_height = 24;
	battery_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w/2-1,layer_height));
	text_layer_set_background_color(battery_layer,GColorBlack);
	text_layer_set_text_color(battery_layer,GColorWhite);
	text_layer_set_font(battery_layer, small_font);
	text_layer_set_text_alignment(battery_layer,GTextAlignmentLeft);
	text_layer_set_text(battery_layer, "bat: --%");
	layer_add_child(root_layer,text_layer_get_layer(battery_layer));

	bluetooth_layer = text_layer_create(GRect(frame.size.w/2+1,layer_accumulator,frame.size.w/2-1,layer_height));
	text_layer_set_background_color(bluetooth_layer,GColorBlack);
	text_layer_set_text_color(bluetooth_layer,GColorWhite);
	text_layer_set_font(bluetooth_layer, small_font);
	text_layer_set_text_alignment(bluetooth_layer,GTextAlignmentRight);
	text_layer_set_text(bluetooth_layer, "blu: n.a.");
	layer_add_child(root_layer,text_layer_get_layer(bluetooth_layer));
	layer_accumulator += layer_height;

	update_display();

	tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
	battery_state_service_subscribe(&update_battery);
	bluetooth_connection_service_subscribe(&update_bluetooth);
}

static void deinit(void) {
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	text_layer_destroy(time_layer);
	text_layer_destroy(date_layer);
	text_layer_destroy(battery_layer);
	window_destroy(window);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
}
