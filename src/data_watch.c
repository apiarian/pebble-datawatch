#include <pebble.h>
#include "suncalc.h"
enum {
	GPS_REQUEST,
	GPS_LAT_RESPONSE,
	GPS_LON_RESPONSE,
	GPS_AUX_RESPONSE,
	GPS_UTC_OFFSET_RESPONSE
};

const int location_decimals = 1e4;
const time_t location_expiration = (60*1)+30;
const uint32_t lat_key = 0;
const uint32_t lon_key = 1;
const uint32_t utc_key = 2;

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
static TextLayer *timer_layer;
static int lat;
static int lon;
static int utc_offset;
static time_t location_update_time;
static int location_update_count;
static time_t timer_start;


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
}

void adjustTimezone(float* time) {
	//int corrected_time = 12 + tz;
	//if(daylight_savings) {
	//	corrected_time += 1;
	//}
	*time += 12 - utc_offset;
	if(*time > 24) *time -= 24;
	if(*time < 0) *time += 24;
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

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	float sunriseTime = calcSunRise(t->tm_year, t->tm_mon+1, t->tm_mday, 1.0*lat/location_decimals, 1.0*lon/location_decimals, ZENITH_OFFICIAL); 
	float sunsetTime = calcSunSet(t->tm_year, t->tm_mon+1, t->tm_mday, 1.0*lat/location_decimals, 1.0*lon/location_decimals, ZENITH_OFFICIAL); 
	adjustTimezone(&sunriseTime);
	adjustTimezone(&sunsetTime);
	struct tm sunrize = {0, (int)(60*(sunriseTime-((int)(sunriseTime)))), (int)sunriseTime-12, 0, 0, 0, 0, 0, 0, 0, 0};
	struct tm sunset = {0, (int)(60*(sunsetTime-((int)(sunsetTime)))), (int)sunsetTime+12, 0, 0, 0, 0, 0, 0, 0, 0};
	static char sunrize_text[] = "00:00";
	static char sunset_text[] = "00:00";
	strftime(sunrize_text, sizeof(sunrize_text), "%H:%M", &sunrize);
	strftime(sunset_text, sizeof(sunset_text), "%H:%M", &sunset);
	text_layer_set_text(sunrize_layer, sunrize_text);
	text_layer_set_text(sunset_layer, sunset_text);
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

	int y, m;
	double jd;
	int jdn;
	y = t->tm_year + 1900;
	m = t->tm_mon + 1;
	jdn = t->tm_mday-32075+1461*(y+4800+(m-14)/12)/4+367*(m-2-(m-14)/12*12)/12-3*((y+4900+(m-14)/12)/100)/4;
	jd = jdn-2451550.1;
	jd /= 29.530588853;
	jd -= (int)jd;
	if(jd<1.0/16)        text_layer_set_text(moonprogress_layer, "new");
	else if (jd<3.0/16)  text_layer_set_text(moonprogress_layer, "wx c");
	else if (jd<5.0/16)  text_layer_set_text(moonprogress_layer, "f qt");
	else if (jd<7.0/16)  text_layer_set_text(moonprogress_layer, "wx g");
	else if (jd<9.0/16)  text_layer_set_text(moonprogress_layer, "full");
	else if (jd<11.0/16) text_layer_set_text(moonprogress_layer, "wn g");
	else if (jd<13.0/16) text_layer_set_text(moonprogress_layer, "l qt");
	else if (jd<15.0/16) text_layer_set_text(moonprogress_layer, "wn c");
	else                 text_layer_set_text(moonprogress_layer, "new");
}

static void out_sent_handler(DictionaryIterator *sent, void *context) {
	//outgoing message delivered
}
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	//outgoing message failed
	switch(reason) {
		case APP_MSG_SEND_TIMEOUT:
			text_layer_set_text(locaux_layer, "com timeout"); break;
		case APP_MSG_SEND_REJECTED:
			text_layer_set_text(locaux_layer, "com rejected"); break;
		case APP_MSG_NOT_CONNECTED:
			text_layer_set_text(locaux_layer, "com disconnected"); break;
		case APP_MSG_APP_NOT_RUNNING:
			text_layer_set_text(locaux_layer, "com not running"); break;
		case APP_MSG_BUSY:
			text_layer_set_text(locaux_layer, "com busy"); break;
		default:
			text_layer_set_text(locaux_layer, "com failure"); break;
	}
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
		Tuple *utc_offset_tuple = dict_find(received, GPS_UTC_OFFSET_RESPONSE);
		if(utc_offset_tuple) {
			utc_offset = utc_offset_tuple->value->int32;
		}
		time(&location_update_time);
		persist_write_int(lat_key, lat);
		persist_write_int(lon_key, lon);
		persist_write_int(utc_key, utc_offset);
	}
	update_location();
}
static void in_dropped_handler(AppMessageResult reason, void *context) {
	//incoming dropped
}

static void handle_tick(struct tm* tick_time, TimeUnits unit_changed) {
	if(unit_changed & MINUTE_UNIT) {
		update_display();
		if(location_update_count>=5){
			send_gps_request();
			location_update_count = 0;
		} else {
			location_update_count += 1;
		}
	}
	static char timer_text[] = "00:00:00";
	int elapsed = (int) (time(NULL) - timer_start);
	int hours = elapsed / 3600;
	int minutes = (elapsed - hours*3600) / 60;
	int seconds = elapsed - hours*3600 - minutes*60;
	struct tm t = {seconds, minutes, hours, 0, 0, 0, 0, 0, 0, 0, 0};
	strftime(timer_text, sizeof(timer_text), "%H:%M:%S", &t);
	//snprintf(timer_text, sizeof(timer_text), "%d:%d:%d", hours, minutes, seconds);
	text_layer_set_text(timer_layer, timer_text);
}

static void handle_tap(AccelAxisType axis, int32_t direction) {
//	static char tap_text[] = "1234567890123";
//	switch(axis) {
//		case ACCEL_AXIS_X:
//			snprintf(tap_text, sizeof(tap_text), "tap X %d", (int)direction); break;
//		case ACCEL_AXIS_Y:
//			snprintf(tap_text, sizeof(tap_text), "tap Y %d", (int)direction); break;
//		case ACCEL_AXIS_Z:
//			snprintf(tap_text, sizeof(tap_text), "tap Z %d", (int)direction); break;
//	}
//	text_layer_set_text(timer_layer, tap_text);
	text_layer_set_text(timer_layer, "00:00:00");
	time(&timer_start);
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
	timer_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w,layer_height));
	text_layer_set_background_color(timer_layer, GColorBlack);
	text_layer_set_text_color(timer_layer, GColorWhite);
	text_layer_set_font(timer_layer, small_font);
	text_layer_set_text_alignment(timer_layer, GTextAlignmentCenter);
	text_layer_set_text(timer_layer, "00:00:00");
	layer_add_child(root_layer, text_layer_get_layer(timer_layer));
	layer_accumulator += layer_height;

	layer_height = 22;
	sunrize_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(sunrize_layer, GColorBlack);
	text_layer_set_text_color(sunrize_layer, GColorWhite);
	text_layer_set_font(sunrize_layer, small_font);
	text_layer_set_text_alignment(sunrize_layer, GTextAlignmentLeft);
	text_layer_set_text(sunrize_layer, "06:00");
	layer_add_child(root_layer, text_layer_get_layer(sunrize_layer));

	moonprogress_layer = text_layer_create(GRect(frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(moonprogress_layer, GColorBlack);
	text_layer_set_text_color(moonprogress_layer, GColorWhite);
	text_layer_set_font(moonprogress_layer, small_font);
	text_layer_set_text_alignment(moonprogress_layer, GTextAlignmentCenter);
	text_layer_set_text(moonprogress_layer, "100%+");
	layer_add_child(root_layer, text_layer_get_layer(moonprogress_layer));

	sunset_layer = text_layer_create(GRect(2*frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
	text_layer_set_background_color(sunset_layer, GColorBlack);
	text_layer_set_text_color(sunset_layer, GColorWhite);
	text_layer_set_font(sunset_layer, small_font);
	text_layer_set_text_alignment(sunset_layer, GTextAlignmentRight);
	text_layer_set_text(sunset_layer, "20:00");
	layer_add_child(root_layer, text_layer_get_layer(sunset_layer));
	layer_accumulator += layer_height;

//	layer_height = 22;
//	moonrise_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w/3,layer_height));
//	text_layer_set_background_color(moonrise_layer, GColorBlack);
//	text_layer_set_text_color(moonrise_layer, GColorWhite);
//	text_layer_set_font(moonrise_layer, small_font);
//	text_layer_set_text_alignment(moonrise_layer, GTextAlignmentLeft);
//	text_layer_set_text(moonrise_layer, "06:00");
//	layer_add_child(root_layer, text_layer_get_layer(moonrise_layer));
//
//	moonprogress_layer = text_layer_create(GRect(frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
//	text_layer_set_background_color(moonprogress_layer, GColorBlack);
//	text_layer_set_text_color(moonprogress_layer, GColorWhite);
//	text_layer_set_font(moonprogress_layer, small_font);
//	text_layer_set_text_alignment(moonprogress_layer, GTextAlignmentCenter);
//	text_layer_set_text(moonprogress_layer, "100%+");
//	layer_add_child(root_layer, text_layer_get_layer(moonprogress_layer));
//
//	moonset_layer = text_layer_create(GRect(2*frame.size.w/3,layer_accumulator,frame.size.w/3,layer_height));
//	text_layer_set_background_color(moonset_layer, GColorBlack);
//	text_layer_set_text_color(moonset_layer, GColorWhite);
//	text_layer_set_font(moonset_layer, small_font);
//	text_layer_set_text_alignment(moonset_layer, GTextAlignmentRight);
//	text_layer_set_text(moonset_layer, "20:00");
//	layer_add_child(root_layer, text_layer_get_layer(moonset_layer));
//	layer_accumulator += layer_height;

	layer_height = 22;
	location_layer = text_layer_create(GRect(0,layer_accumulator,frame.size.w,layer_height));
	text_layer_set_background_color(location_layer, GColorBlack);
	text_layer_set_text_color(location_layer, GColorWhite);
	text_layer_set_font(location_layer, small_font);
	text_layer_set_text_alignment(location_layer, GTextAlignmentCenter);
	text_layer_set_text(location_layer, "+0.0000 +0.0000");
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
	if(persist_exists(lon_key)) {
		lon = persist_read_int(lon_key);
		text_layer_set_text(locaux_layer, "cached data");
	}
	else
		lon = 0;
	if(persist_exists(utc_key))
		utc_offset = persist_read_int(utc_key);
	else
		utc_offset = 0;
	update_location();

	update_display();

	update_battery(battery_state_service_peek());
	update_bluetooth(bluetooth_connection_service_peek());

	time(&timer_start);

	tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
	battery_state_service_subscribe(&update_battery);
	bluetooth_connection_service_subscribe(&update_bluetooth);
	accel_tap_service_subscribe(&handle_tap);

	send_gps_request();
}

static void deinit(void) {
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	accel_tap_service_unsubscribe();
	app_message_deregister_callbacks();
	text_layer_destroy(time_layer);
	text_layer_destroy(date_layer);
	text_layer_destroy(sunrize_layer);
	text_layer_destroy(sunset_layer);
	text_layer_destroy(timer_layer);
//	text_layer_destroy(moonrise_layer);
//	text_layer_destroy(moonprogress_layer);
//	text_layer_destroy(moonset_layer);
	text_layer_destroy(location_layer);
	text_layer_destroy(locaux_layer);
	text_layer_destroy(battery_layer);
	text_layer_destroy(bluetooth_layer);
	window_destroy(window);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
}
