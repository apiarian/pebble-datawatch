#include <pebble.h>

enum {
	GPS_REQUEST,
	GPS_LAT_RESPONSE,
	GPS_LON_RESPONSE,
	GPS_AUX_RESPONSE
};

const int location_decimals = 1e4;
const time_t location_expiration = (60*1)+30;
const uint32_t lat_key = 0;
const uint32_t lon_key = 1;

static Window *window;
static TextLayer *time_layer;
static TextLayer *date_layer;
static TextLayer *battery_layer;
static TextLayer *bluetooth_layer;
static TextLayer *location_layer;
static TextLayer *locaux_layer;
static TextLayer *sunrize_layer;
static TextLayer *sunset_layer;
static TextLayer *sun_layer;
static TextLayer *moonrise_layer;
static TextLayer *moonset_layer;
static TextLayer *moonprogress_layer;
static int lat;
static int lon;
static time_t location_update_time;
//static bool last_bluetooth;


static void send_gps_request(){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	Tuplet value = TupletInteger(GPS_REQUEST,1);
	dict_write_tuplet(iter, &value);
	app_message_outbox_send();
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
//	if(connected && !last_bluetooth)
//		send_gps_request();
//	last_bluetooth = connected;
}

static void update_location() {
	static char location_text[] = "+12.1234 -123.1234";
	int lat_a = lat/location_decimals;
	int lat_b = lat-lat_a*location_decimals;
	lat_b = lat_b>0 ? lat_b : -1*lat_b;
	int lon_a = lon/location_decimals;
	int lon_b = lon-lon_a*location_decimals;
	lon_b = lon_b>0 ? lon_b : -1*lon_b;
	snprintf(location_text, sizeof(location_text), "%+d.%d %+d.%d", lat_a, lat_b, lon_a, lon_b);
	if( (time(NULL) - location_update_time) > location_expiration )
		text_layer_set_font(location_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	else
		text_layer_set_font(location_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text(location_layer, location_text);
}

static void update_display() {
	time_t now = time(NULL);
	const struct tm *t = localtime(&now);

	static char time_text[] = "00:00";
	static char date_text[] = "wkd YYYY-MM-DD.....";

	clock_copy_time_string(time_text, sizeof(time_text));
	text_layer_set_text(time_layer,time_text);

	strftime(date_text, sizeof(date_text), "%a-%F", t);
	text_layer_set_text(date_layer,date_text);
}

static void out_sent_handler(DictionaryIterator *sent, void *context) {
	//outgoing message delivered
}
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	//outgoing message failed
	text_layer_set_text(locaux_layer, "com failure");
	update_location();
}
static void in_received_handler(DictionaryIterator *received, void *context) {
	char errorflag = 0;
	Tuple *lat_tuple = dict_find(received, GPS_LAT_RESPONSE);
	if(lat_tuple) {
		if(lat_tuple->value->int32 != 99)
			lat = lat_tuple->value->int32;
		else
			errorflag = 1;
	}
	Tuple *lon_tuple = dict_find(received, GPS_LON_RESPONSE);
	if(lon_tuple) {
		if(!errorflag)
			lon = lon_tuple->value->int32;
		else {
			switch(lon_tuple->value->int32) {
				case 1:
					text_layer_set_text(locaux_layer, "permission denied");
					break;
				case 2:
					text_layer_set_text(locaux_layer, "unavailable");
					break;
				case 3:
					text_layer_set_text(locaux_layer, "timeout");
					break;
			}
		}
	}
	if(!errorflag) {
		Tuple *aux_tuple = dict_find(received, GPS_AUX_RESPONSE);
		if(aux_tuple) {
			text_layer_set_text(locaux_layer, aux_tuple->value->cstring);
		}
		time(&location_update_time);
		persist_write_int(lat_key, lat);
		persist_write_int(lon_key, lon);
	}
	update_location();
}
static void in_dropped_handler(AppMessageResult reason, void *context) {
	//incoming dropped
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits unit_changed) {
	update_display();
	send_gps_request();
}


static void init(void) {
	GFont time_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	GFont mid_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
	GFont tiny_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

	window = window_create();
	const bool animated = true;
	window_stack_push(window, animated);

	window_set_background_color(window,GColorWhite);
	Layer *root_layer = window_get_root_layer(window);
	GRect frame = layer_get_frame(root_layer);

	int layer_height = 30;
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

	layer_height = 22;
	int sun_width = 22;
	sunrize_layer = text_layer_create(GRect(0,layer_accumulator,(frame.size.w-sun_width)/2,layer_height));
	text_layer_set_background_color(sunrize_layer, GColorBlack);
	text_layer_set_text_color(sunrize_layer, GColorWhite);
	text_layer_set_font(sunrize_layer, small_font);
	text_layer_set_text_alignment(sunrize_layer, GTextAlignmentLeft);
	text_layer_set_text(sunrize_layer, "06:00");
	layer_add_child(root_layer, text_layer_get_layer(sunrize_layer));

	sunset_layer = text_layer_create(GRect((frame.size.w+sun_width)/2,layer_accumulator,(frame.size.w-sun_width)/2,layer_height));
	text_layer_set_background_color(sunset_layer, GColorBlack);
	text_layer_set_text_color(sunset_layer, GColorWhite);
	text_layer_set_font(sunset_layer, small_font);
	text_layer_set_text_alignment(sunset_layer, GTextAlignmentRight);
	text_layer_set_text(sunset_layer, "20:00");
	layer_add_child(root_layer, text_layer_get_layer(sunset_layer));
	layer_accumulator += layer_height;

	layer_height = 22;
	moonrise_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(moonrise_layer, GColorBlack);
	text_layer_set_text_color(moonrise_layer, GColorWhite);
	text_layer_set_font(moonrise_layer, small_font);
	text_layer_set_text_alignment(moonrise_layer, GTextAlignmentLeft);
	text_layer_set_text(moonrise_layer, "06:00");
	layer_add_child(root_layer, text_layer_get_layer(moonrise_layer));

	moonprogress_layer = text_layer_create(GRect(frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(moonprogress_layer, GColorBlack);
	text_layer_set_text_color(moonprogress_layer, GColorWhite);
	text_layer_set_font(moonprogress_layer, small_font);
	text_layer_set_text_alignment(moonprogress_layer, GTextAlignmentCenter);
	text_layer_set_text(moonprogress_layer, "100%+");
	layer_add_child(root_layer, text_layer_get_layer(moonprogress_layer));

	moonset_layer = text_layer_create(GRect(2*frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(moonset_layer, GColorBlack);
	text_layer_set_text_color(moonset_layer, GColorWhite);
	text_layer_set_font(moonset_layer, small_font);
	text_layer_set_text_alignment(moonset_layer, GTextAlignmentRight);
	text_layer_set_text(moonset_layer, "20:00");
	layer_add_child(root_layer, text_layer_get_layer(moonset_layer));
	layer_accumulator += layer_height;

	layer_height = 22;
	location_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w,layer_height));
	text_layer_set_background_color(location_layer, GColorBlack);
	text_layer_set_text_color(location_layer, GColorWhite);
	text_layer_set_font(location_layer, small_font);
	text_layer_set_text_alignment(location_layer, GTextAlignmentCenter);
	text_layer_set_text(location_layer, "+99.0000 +0.0000");
	layer_add_child(root_layer, text_layer_get_layer(location_layer));
	layer_accumulator += layer_height;

	layer_height = 22;
	locaux_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w,layer_height));
	text_layer_set_background_color(locaux_layer, GColorBlack);
	text_layer_set_text_color(locaux_layer, GColorWhite);
	text_layer_set_font(locaux_layer, small_font);
	text_layer_set_text_alignment(locaux_layer, GTextAlignmentCenter);
	text_layer_set_text(locaux_layer, "accuracy: n.a.");
	layer_add_child(root_layer, text_layer_get_layer(locaux_layer));
	layer_accumulator += layer_height;

	layer_height = 22;
	battery_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w/2,layer_height));
	text_layer_set_background_color(battery_layer,GColorBlack);
	text_layer_set_text_color(battery_layer,GColorWhite);
	text_layer_set_font(battery_layer, small_font);
	text_layer_set_text_alignment(battery_layer,GTextAlignmentLeft);
	text_layer_set_text(battery_layer, "bat: --%");
	layer_add_child(root_layer,text_layer_get_layer(battery_layer));

	bluetooth_layer = text_layer_create(GRect(frame.size.w/2,layer_accumulator,frame.size.w/2,layer_height));
	text_layer_set_background_color(bluetooth_layer,GColorBlack);
	text_layer_set_text_color(bluetooth_layer,GColorWhite);
	text_layer_set_font(bluetooth_layer, small_font);
	text_layer_set_text_alignment(bluetooth_layer,GTextAlignmentRight);
	text_layer_set_text(bluetooth_layer, "blu: n.a.");
	layer_add_child(root_layer,text_layer_get_layer(bluetooth_layer));
	layer_accumulator += layer_height;

	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);
	const uint32_t inboud_size = 64;
	const uint32_t outbound_size = 64;
	app_message_open(inboud_size, outbound_size);

	if(persist_exists(lat_key))
		lat = persist_read_int(lat_key);
	else
		lat = 0;
	if(persist_exists(lon_key))
		lon = persist_read_int(lon_key);
	else
		lon = 0;
	update_display();

	update_battery(battery_state_service_peek());
//	last_bluetooth = bluetooth_connection_service_peek();
	update_bluetooth(bluetooth_connection_service_peek());

	//tick_timer_service_subscribe(SECOND_UNIT, &handle_minute_tick);
	tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
	battery_state_service_subscribe(&update_battery);
	bluetooth_connection_service_subscribe(&update_bluetooth);

	send_gps_request();
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
