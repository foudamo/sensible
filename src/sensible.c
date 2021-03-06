#include "pebble.h"

Window *window;
TextLayer *text_date_layer;
TextLayer *text_time_layer;
Layer *line_layer;

GBitmap *low_batt_image;
BitmapLayer *low_batt_layer;

GBitmap *bt_disconnect_image;
BitmapLayer *bt_disconnect_layer;


static void handle_bluetooth(bool connected) {
  bitmap_layer_set_bitmap(bt_disconnect_layer, connected ? NULL : bt_disconnect_image);
  if(!connected) vibes_double_pulse();
}

static void handle_battery(BatteryChargeState charge_state) {
  bitmap_layer_set_bitmap(low_batt_layer, charge_state.charge_percent > 20 ? NULL : low_batt_image);
}


void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;

  if (!tick_time) {
    time_t now = time(NULL);
    tick_time = localtime(&now);
  }

  // TODO: Only update the date when it's changed.
  strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
  text_layer_set_text(text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(text_time_layer, time_text);

  // Check for low battery
  handle_battery(battery_state_service_peek());

}

void handle_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();

  layer_remove_from_parent(bitmap_layer_get_layer(low_batt_layer));
  bitmap_layer_destroy(low_batt_layer);
  gbitmap_destroy(low_batt_image);

  layer_remove_from_parent(bitmap_layer_get_layer(bt_disconnect_layer));
  bitmap_layer_destroy(bt_disconnect_layer);
  gbitmap_destroy(bt_disconnect_image);

  text_layer_destroy(text_date_layer);
  text_layer_destroy(text_time_layer);
  layer_destroy(line_layer);

  window_destroy(window);
}

void handle_init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  Layer *window_layer = window_get_root_layer(window);

  text_date_layer = text_layer_create(GRect(8, 68, 144-8, 168-68));
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  text_layer_set_font(text_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

  text_time_layer = text_layer_create(GRect(7, 92, 144-7, 168-92));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

  GRect line_frame = GRect(8, 97, 128, 2);
  line_layer = layer_create(line_frame);
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(window_layer, line_layer);


  low_batt_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LOW_BATT);
  low_batt_layer = bitmap_layer_create(GRect(0,0,16,16));
  bitmap_layer_set_bitmap(low_batt_layer, NULL);
  layer_add_child(window_layer, bitmap_layer_get_layer(low_batt_layer));

  bt_disconnect_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_DISCONNECT);
  bt_disconnect_layer = bitmap_layer_create(GRect(16,0,16,16));
  bitmap_layer_set_bitmap(bt_disconnect_layer, NULL);
  layer_add_child(window_layer, bitmap_layer_get_layer(bt_disconnect_layer));

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  handle_minute_tick(NULL, MINUTE_UNIT);

  // Bluetooth status

  bluetooth_connection_service_subscribe(&handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek());

  // Battery status

  battery_state_service_subscribe(&handle_battery);
  handle_battery(battery_state_service_peek());

}


int main(void) {
  handle_init();

  app_event_loop();
  
  handle_deinit();
}
