// WatchChronometer (c) 2014 - Keith Blom - Allrights reserved
// V1.2 - Save/restore state.

// Standard includes
#include "pebble.h"

// There is only one window for this app.
static Window *window; 

// Layers.
static BitmapLayer *lightLayer;
static TextLayer *modeButtonLayer;
static TextLayer *timeChronoLayer;
static BitmapLayer *ssLayer;
static TextLayer *dateStrInfoLayer;
static TextLayer *resetButtonLayer;

// Mode select
#define MODE_CLOCK 0
#define MODE_CHRON 1
#define MODE_MAX 2
static short selectedMode = MODE_CLOCK;

// Start/Stop select
#define RUN_START 0
#define RUN_STOP 1
#define RUN_MAX 2
static short chronoRunSelect = RUN_STOP;

// Time chronometer was started, or zero if reset.
time_t chronoElapsed = 0;

// Time or chronograph value.
static char timeText[] = "00:00:00";

// Month day.
static char dateStr[] = "Jan 31"; 

// Saved chrono time during wait for reset.
static char savedChronoText[] = "00:00:00"; 
time_t savedChronoElapsed = 0;
bool resetInProgress = false;
AppTimer *resetTimerHandle = NULL;

// Access once and remember
static bool clock_is_24h = false;

// "true" if the Reset label should be hidden when in MODE_CHRON.
static bool hideResetDisplay = true;

// Key to access persistent data.
static const uint32_t  persistent_data_key = 1;	

// Structure to save state when app is not running
typedef struct saved_state_S
{
  short selectedMode;
  char timeText [9];
  char dateStr [7];
  short chronoRunSelect;
  time_t chronoElapsed;
  time_t closeTm;
  bool hideResetDisplay;
} saved_state_S;


// Called once per second
static void handle_second_tick(struct tm *currentTime, TimeUnits units_changed) 
{
  // Maintain a running chronometer whether or not currently being displayed.
  if (chronoRunSelect == RUN_START)
  {
    chronoElapsed += 1;
  }

  if (selectedMode == MODE_CHRON && resetInProgress == false)
  {

    // Limit display to 2 hours digits.
    int real_hours = chronoElapsed/3600;
    int hours = real_hours%100;
    int min = (chronoElapsed - real_hours*3600)/60;
    int sec = (chronoElapsed - real_hours*3600 - min*60);

    // Format.
    snprintf(timeText, sizeof(timeText), "%2i:%02i:%02i", hours, min, sec);

    text_layer_set_text(timeChronoLayer, timeText);
  }
  else if (selectedMode == MODE_CLOCK)
  {
    int hourStyled = currentTime->tm_hour;
    if ( ! clock_is_24h && hourStyled > 12)
    {
      hourStyled -= 12;
    }

    snprintf(timeText, sizeof(timeText), "%i:%02i:%02i", hourStyled,
                                                           currentTime->tm_min,
                                                           currentTime->tm_sec);

    text_layer_set_text(timeChronoLayer, timeText);

    strftime(dateStr, 7, "%b", currentTime);
    snprintf(&(dateStr[3]), 4, " %i",  currentTime->tm_mday);
    text_layer_set_text(dateStrInfoLayer, dateStr);
  }
}


// Mode button
static void up_long_click_handler(ClickRecognizerRef recognizer, Window *window) {

  selectedMode = (selectedMode + 1) % MODE_MAX;

  // Swap display format.
  if (selectedMode == MODE_CHRON)
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), false);
    strncpy(dateStr, "CHRONO", sizeof(dateStr));
    text_layer_set_text(dateStrInfoLayer, dateStr);

    layer_set_hidden(text_layer_get_layer(resetButtonLayer), hideResetDisplay);
  }

  // WATCH mode
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), true);
    layer_set_hidden(text_layer_get_layer(resetButtonLayer), true);
  }
}


// Start/stop button
static void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  if (selectedMode == MODE_CHRON)
  {
    chronoRunSelect = (chronoRunSelect + 1) % RUN_MAX;

    // Transitioned to running.
    if (chronoRunSelect == RUN_START)
    {
      hideResetDisplay = true;
    }
    // Transitioned to stopped.
    else
    {
      hideResetDisplay = false;
    }

    layer_set_hidden(text_layer_get_layer(resetButtonLayer), hideResetDisplay);
  }
}


static void reset_timeout_handler(void *callback_data) {

  chronoElapsed = 0;
  text_layer_set_text(timeChronoLayer, " 0:00:00");
  layer_set_hidden(text_layer_get_layer(resetButtonLayer), true);
  hideResetDisplay = true;

  resetInProgress = false;
}


// Reset button. Must be displaying chrono, not running, and needing to be reset.
static void down_down_handler(ClickRecognizerRef recognizer, Window *window) {

  // Must be displaying chrono, not running, and needing to be reset.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_STOP && hideResetDisplay == false)
  {
    resetTimerHandle = app_timer_register(1000, reset_timeout_handler, NULL);

    strcpy(savedChronoText, text_layer_get_text(timeChronoLayer));
    text_layer_set_text(timeChronoLayer, "HOLD");
    savedChronoElapsed = chronoElapsed;

    resetInProgress = true;
  }
}


static void down_up_handler(ClickRecognizerRef recognizer, Window *window) {

  // Must be displaying chrono, not running, and needing to be reset.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_STOP && hideResetDisplay == false)
  {
    // Reset timeout fired, meaning we proceed with the reset.
    if (resetInProgress == false)
    {
      chronoElapsed = 0;
      text_layer_set_text(timeChronoLayer, " 0:00:00");
      layer_set_hidden(text_layer_get_layer(resetButtonLayer), true);
      hideResetDisplay = true;
    }
    // Button release before reset timeout completed - abort reset!
    else
    {
      text_layer_set_text(timeChronoLayer, savedChronoText);
      chronoElapsed = savedChronoElapsed;

      app_timer_cancel(resetTimerHandle);
      resetTimerHandle = NULL;
    }

    resetInProgress = false;
  }
}


static void click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "entered click_config_provider");

  window_long_click_subscribe(BUTTON_ID_UP, 500, (ClickHandler) up_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_single_click_handler);
  window_raw_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) down_down_handler, (ClickHandler) down_up_handler, 

NULL);
}


static void app_init() {

  window = window_create();
  Layer *window_layer = window_get_root_layer(window);
  window_set_fullscreen(window, true);
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  // Restore state if exists.
  if (persist_exists(persistent_data_key))
  {
    saved_state_S saved_state;
    int bytes_read = 0;
    if (sizeof(saved_state_S) == (bytes_read = persist_read_data(persistent_data_key,
                                                                 (void *)&saved_state,
                                                                 sizeof(saved_state_S))))
    {
      //(APP_LOG_LEVEL_DEBUG, "read state: selectedMode: %i, chronoRunSelect: %i, closeTm: %i",
      //                             saved_state.selectedMode,
      //                             saved_state.chronoRunSelect,
      //                             (int)saved_state.closeTm);

      // Chronometer time that elapsed while app was not running.
      time_t timeSinceClosed = 0;
      if (saved_state.chronoRunSelect == RUN_START)
      {
        timeSinceClosed = time(NULL) - saved_state.closeTm;
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "timeSinceClosed: %i", (int)timeSinceClosed);
      }

      selectedMode = saved_state.selectedMode;
      strncpy(timeText, saved_state.timeText, sizeof(timeText));
      strncpy(dateStr, saved_state.dateStr, sizeof(dateStr)); 
      chronoRunSelect = saved_state.chronoRunSelect;
      chronoElapsed = saved_state.chronoElapsed + timeSinceClosed;
      hideResetDisplay = saved_state.hideResetDisplay;
    }
    else
    {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_read_data(). bytes read = %i", bytes_read);
    }
  }
  else
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists() returned false");
  }

  // Font for time and chronometer.
  GFont custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_36));

  // Light icon area
  GBitmap *light_image = gbitmap_create_with_resource(RESOURCE_ID_LIGHT_ICON);
  lightLayer = bitmap_layer_create(GRect(84, 4, 16, 16));
  bitmap_layer_set_bitmap(lightLayer, light_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(lightLayer));

  // Mode button
  modeButtonLayer = text_layer_create(GRect(100, 0, 44, 20));
  text_layer_set_text_alignment(modeButtonLayer, GTextAlignmentRight);
  text_layer_set_font(modeButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(modeButtonLayer, GColorBlack);
  text_layer_set_text_color(modeButtonLayer, GColorWhite);
  text_layer_set_text(modeButtonLayer, "/Mode");
  layer_add_child(window_layer, text_layer_get_layer(modeButtonLayer));

  // Time/chronograph area
  timeChronoLayer = text_layer_create(GRect(12, 61, 118, 38));
  text_layer_set_text_alignment(timeChronoLayer, GTextAlignmentCenter);
  text_layer_set_font(timeChronoLayer, custom_font);
  text_layer_set_background_color(timeChronoLayer, GColorBlack);
  text_layer_set_text_color(timeChronoLayer, GColorWhite);
  text_layer_set_text(timeChronoLayer, timeText);
  layer_add_child(window_layer, text_layer_get_layer(timeChronoLayer));

  // Start/stop area
  GBitmap *ss_image = gbitmap_create_with_resource(RESOURCE_ID_START_STOP);
  ssLayer = bitmap_layer_create(GRect(130, 67, 14, 30));
  bitmap_layer_set_bitmap(ssLayer, ss_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(ssLayer));
  if (selectedMode == MODE_CHRON)
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), false);
  }
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), true);
  }

  // dateStr/info area
  dateStrInfoLayer = text_layer_create(GRect(0, 101, 144, 20));
  text_layer_set_text_alignment(dateStrInfoLayer, GTextAlignmentCenter);
  text_layer_set_font(dateStrInfoLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(dateStrInfoLayer, GColorBlack);
  text_layer_set_text_color(dateStrInfoLayer, GColorWhite);
  text_layer_set_text(dateStrInfoLayer, dateStr);
  layer_add_child(window_layer, text_layer_get_layer(dateStrInfoLayer));

  // Reset button
  resetButtonLayer = text_layer_create(GRect(0, 151, 144, 20));
  text_layer_set_text_alignment(resetButtonLayer, GTextAlignmentRight);
  text_layer_set_font(resetButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(resetButtonLayer, GColorBlack);
  text_layer_set_text_color(resetButtonLayer, GColorWhite);
  text_layer_set_text(resetButtonLayer, "Reset");
  layer_add_child(window_layer, text_layer_get_layer(resetButtonLayer));
  if (selectedMode == MODE_CHRON)
  {
    layer_set_hidden(text_layer_get_layer(resetButtonLayer), hideResetDisplay);
  }
  else
  {
    layer_set_hidden(text_layer_get_layer(resetButtonLayer), true);
  }

  window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);

  clock_is_24h = clock_is_24h_style();
  
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}


static void app_deinit() {

  // Save state.
  saved_state_S saved_state;
  saved_state.selectedMode = selectedMode;
  strncpy(saved_state.timeText, timeText, sizeof(saved_state.timeText));
  strncpy(saved_state.dateStr, dateStr, sizeof(saved_state.dateStr)); 
  saved_state.chronoRunSelect = chronoRunSelect;
  saved_state.chronoElapsed = chronoElapsed;
  saved_state.closeTm = time(NULL);
  saved_state.hideResetDisplay = hideResetDisplay;

  int bytes_written = 0;
  if (sizeof(saved_state_S) != (bytes_written = persist_write_data(persistent_data_key,
                                                                   (void *)&saved_state,
                                                                   sizeof(saved_state_S))))
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_write_data(). bytes written = %i", bytes_written);

    // Delete peristent data when a problem has occurred saving it.
    persist_delete(persistent_data_key);
  }
  else
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "write state: selectedMode: %i, chronoRunSelect: %i, closeTm: %i",
    //                             saved_state.selectedMode,
    //                             saved_state.chronoRunSelect,
    //                             (int)saved_state.closeTm);
  }

  if (resetTimerHandle != NULL)
  {
      app_timer_cancel(resetTimerHandle);
  }

  bitmap_layer_destroy(lightLayer);
  text_layer_destroy(modeButtonLayer);
  text_layer_destroy(timeChronoLayer);
  text_layer_destroy(dateStrInfoLayer);
  bitmap_layer_destroy(ssLayer);
  text_layer_destroy(resetButtonLayer);
  window_destroy(window);
}


int main(void) {
	app_init();
  app_event_loop();
	app_deinit();
}