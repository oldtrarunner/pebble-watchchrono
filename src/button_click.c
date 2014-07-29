// WatchChronometer (c) 2014 Keith Blom - All rights reserved
// V2 - Add splits.

#define APP_VERSION "Ver 2.0.0"

// Standard includes
#include "pebble.h"

// Forward declarations.
void format_splits_content();
void setup_splits_window();
void select_splits_display_content();
//void setup_time_chrono_window();
//static void splits_click_config_provider(Window *window);

// Menu window is pushed to the stack first, then the time window below it.
// Splits window is pushed when selected from menu.
static Window *menu_window; 
static Window *split_window; 
static Window *time_window; 

// Menu window layers and associated data.
#define NBR_MENU_ITEMS 4
static SimpleMenuLayer *menuLayer;
static SimpleMenuItem menuItems[NBR_MENU_ITEMS];
static SimpleMenuSection menuSection[1];

// Splits window layers.
static TextLayer *splitTitleLayer;
static TextLayer *splitSepLayer;
static TextLayer *splitContentLayer;

// Time/chronometer window layers.
static BitmapLayer *lightLayer;
static TextLayer *modeButtonLayer;
static TextLayer *timeChronoLayer;
static BitmapLayer *ssLayer;
static TextLayer *dateInfoLayer;
static TextLayer *sptRstButtonLayer;

// Display mode select
#define MODE_CLOCK 0
#define MODE_CHRON 1
#define MODE_MAX 2
static short selectedMode = MODE_CLOCK;

// Start/Stop select
#define RUN_START 0
#define RUN_STOP 1
#define RUN_MAX 2
static short chronoRunSelect = RUN_STOP;

// Splits button behavior.
// Make this a user selectable option some day.
#define SPLITS_KEEP_OLDEST 0
#define SPLITS_KEEP_LATEST 1
static int splitButtonBehavior = SPLITS_KEEP_OLDEST;

// Font for time and chronmeter.
GFont time_font;

// Graphics
GBitmap* menuIcon;
GBitmap *light_image;
GBitmap *ss_image;

// Time chronometer was started, or zero if reset.
static time_t chronoElapsed = 0;

// Time or chronograph value as text string.
static char timeText[] = "00:00:00";

// Month day or "CHRONO".
static char dateStr[] = "Jan 31"; 

// Saved chrono time during wait for reset.
static char savedChronoText[] = "00:00:00"; 
static time_t savedChronoElapsed = 0;
static bool resetInProgress = false;
static AppTimer *resetTimerHandle = NULL;

// Access once and remember.
static bool clock_is_24h = false;

// "true" if the chronometer has been reset after being stopped.
// (i.e. do not need to display RESET text on a stopped chronometer)
static bool chronoHasBeenReset = true;

// Split/reset button text.
#define SPLIT_TEXT_MAX_LEN 11
static char BLANK_TEXT[] = "";
static char RESET_TEXT[] = "Reset";
static char SPLIT_TEXT[] = "Split";     // Will be appended with next available split number.
static char SPLIT_TEXT_FULL[] = "Split Full";
static char spt_rstButtonText[SPLIT_TEXT_MAX_LEN] = ""; // Space for "Split Full"

// Key to access persistent data.
static const uint32_t  persistent_data_key = 1;

// Splits. Earliest first. Split format " 1) 12:34:56" plus newline/null.
#define SPLIT_INDEX_RESET -1
#define MAX_SPLIT_INDEX 29
#define MAX_DISPLAY_SPLITS 5
#define CHARS_PER_SPLIT 13
static time_t splits[MAX_SPLIT_INDEX + 1];
static int splitIndex = SPLIT_INDEX_RESET;
static char formattedSplits[(MAX_SPLIT_INDEX + 1) * CHARS_PER_SPLIT];
static char splitsDisplayContent[MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT + 1]; // Extra char needed for \0.
static char SPLITS_DISPLAY_NONE[] = "    None    "; // Must be CHARS_PER_SPLIT including NULL.
static int splitDisplayIndex = 0;

// Structure to save state when app is not running.
typedef struct saved_state_S
{
  short selectedMode;
  char timeText [9];
  char dateStr [7];
  short chronoRunSelect;
  time_t chronoElapsed;
  time_t closeTm;
  char spt_rstButtonText[SPLIT_TEXT_MAX_LEN];
  bool chronoHasBeenReset;
  time_t split_1;
  time_t splits[MAX_SPLIT_INDEX + 1];
  int splitIndex;
} __attribute__((__packed__)) saved_state_S;

//##################### Menu window support ################################

static void menuWatchChronoHandler(int index, void *context)
{
  window_stack_push(time_window, true /* Animated */);
}


static void menuDisplaySplitsHandler(int index, void *context)
{
  format_splits_content();

  splitDisplayIndex = 0;
  select_splits_display_content();

  text_layer_set_text(splitContentLayer, splitsDisplayContent);
  window_stack_push(split_window, true /* Animated */);
}


static void menuClearSplitsHandler(int index, void *context)
{
  strcpy(formattedSplits, SPLITS_DISPLAY_NONE);

  // Clear splits.
  splitIndex = -1;
  splitDisplayIndex = 0;
  select_splits_display_content();

  // If the chronometer is running and was the last thing displayed before
  // navigation to Clear splits, need to label Splits button for split 1.
  if (chronoRunSelect == RUN_START)
  {
    snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 2);
  }

  text_layer_set_text(splitContentLayer, splitsDisplayContent);
  window_stack_push(split_window, true /* Animated */);
}

//##################### Split window support ################################


void format_splits_content(){

  if (splitIndex >= 0)
  {
    int real_hours = splits[0]/3600;
    int hours = real_hours%100;
    int min = (splits[0] - real_hours*3600)/60;
    int sec = (splits[0] - real_hours*3600 - min*60);
    snprintf(formattedSplits, sizeof(formattedSplits), " 1)  %i:%02i:%02i", hours, min, sec);

    bool moreSplits = true;
    for  (int i = 1; i <= splitIndex && moreSplits; i++)
    {
      if (splits[i] > 0)
      {
        real_hours = splits[i]/3600;
        hours = real_hours%100;
        min = (splits[i] - real_hours*3600)/60;
        sec = (splits[i] - real_hours*3600 - min*60);

        // Format. Each line contains one split and must be exactly CHARS_PER_SPLIT wide.
        // "blanks" fill does not seem to work for integers, so do it manually.
        int oneBasedCnt = i + 1;
        if (oneBasedCnt < 10)
        {
          if (hours < 10)
          {
            snprintf(formattedSplits, sizeof(formattedSplits), "%s\n %i)  %i:%02i:%02i", formattedSplits, oneBasedCnt, hours, min, sec);
          }
          else
          {
            snprintf(formattedSplits, sizeof(formattedSplits), "%s\n %i) %i:%02i:%02i", formattedSplits, oneBasedCnt, hours, min, sec);
          }
        }
        else
        {
          if (hours < 10)
          {
            snprintf(formattedSplits, sizeof(formattedSplits), "%s\n%i)  %i:%02i:%02i", formattedSplits, oneBasedCnt, hours, min, sec);
          }
          else
          {
            snprintf(formattedSplits, sizeof(formattedSplits), "%s\n%i) %i:%02i:%02i", formattedSplits, oneBasedCnt, hours, min, sec);
          }
        }
      }
      else
      {
        moreSplits = false;
      }
    }
  }
  else
  {
    strcpy(formattedSplits, SPLITS_DISPLAY_NONE);
  }
}


void select_splits_display_content() {

  // Get up to MAX_DISPLAY_SPLITS rows beginning at splitDisplayIndex.
  strncpy(splitsDisplayContent,
          &(formattedSplits[splitDisplayIndex * CHARS_PER_SPLIT]),
          MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT);

  // Replace trailing \n with \0.
  memset(splitsDisplayContent + (MAX_DISPLAY_SPLITS * CHARS_PER_SPLIT), 0, 1);
}


// Splits UP button. Scroll up a whole page.
static void splits_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // If not on first page, scroll back a page.
  if (splitDisplayIndex - MAX_DISPLAY_SPLITS >= 0)
  {
    splitDisplayIndex -= MAX_DISPLAY_SPLITS;

    select_splits_display_content();

    text_layer_set_text(splitContentLayer, splitsDisplayContent);
  }
}


// Splits DOWN button. Scroll down a whole page.
static void splits_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // If not on last page, scroll forward a page.
  int lastIndexOnDisplay = splitDisplayIndex + MAX_DISPLAY_SPLITS - 1;
  if (lastIndexOnDisplay < splitIndex)
  {
    splitDisplayIndex += MAX_DISPLAY_SPLITS;

    select_splits_display_content();

    text_layer_set_text(splitContentLayer, splitsDisplayContent);
  }
}


// Splits click configuration.
static void splits_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) splits_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) splits_down_single_click_handler);
}

//##################### Time/chrono window support ##########################

// Used by time/chronometer window. Called once per second.
static void tc_handle_second_tick(struct tm *currentTime, TimeUnits units_changed) 
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

    snprintf(timeText, sizeof(timeText), "%2i:%02i:%02i", hourStyled,
                                                           currentTime->tm_min,
                                                           currentTime->tm_sec);

    text_layer_set_text(timeChronoLayer, timeText);

    strftime(dateStr, 7, "%b", currentTime);
    snprintf(&(dateStr[3]), 4, " %i",  currentTime->tm_mday);
    text_layer_set_text(dateInfoLayer, dateStr);
  }
}


// Time/chronometer window Mode button.
static void tc_up_long_click_handler(ClickRecognizerRef recognizer, Window *window) {

  selectedMode = (selectedMode + 1) % MODE_MAX;

  // CHRONO mode
  if (selectedMode == MODE_CHRON)
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), false);

    strncpy(dateStr, "CHRONO", sizeof(dateStr));
    text_layer_set_text(dateInfoLayer, dateStr);

    if (chronoRunSelect == RUN_START)
    {
      // Splits buffer not full.
      if (splitIndex < MAX_SPLIT_INDEX)
      {
        // Label split button wth next available split buffer slot number.
        // splitIndex is set to last used, so increment by 2: 1 to make count + 1 to make next
        snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 2);
      }

      // Splits buffer is full.
      else
      {
        // Mark full when keeping oldest splits.
        if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s", SPLIT_TEXT_FULL);
        }

        // Label with max split count when keeping latest splits.
        else
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, MAX_SPLIT_INDEX + 1);
        }
      }

      //strncpy(spt_rstButtonText, SPLIT_TEXT, sizeof(spt_rstButtonText));
    }
    else if ( ! chronoHasBeenReset)
    {
      strncpy(spt_rstButtonText, RESET_TEXT, sizeof(spt_rstButtonText));
    }
    // else already BLANK from watch display

    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }

  // WATCH mode
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), true);

    strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }
}


// Time/chronometer window Start/Stop button
static void tc_select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  if (selectedMode == MODE_CHRON)
  {
    chronoRunSelect = (chronoRunSelect + 1) % RUN_MAX;

    // Transitioned to running. Display Splits button.
    if (chronoRunSelect == RUN_START)
    {
      //strncpy(spt_rstButtonText, SPLIT_TEXT, sizeof(spt_rstButtonText));

      // Splits buffer not full.
      if (splitIndex < MAX_SPLIT_INDEX)
      {
        // Label split button wth next available split buffer slot number.
        // splitIndex is set to last used, so increment by 2: 1 to make count + 1 to make next
        snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 2);
      }

      // Splits buffer is full.
      else
      {
        // Mark full when keeping oldest splits.
        if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s", SPLIT_TEXT_FULL);
        }

        // Label with max split count when keeping latest splits.
        else
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, MAX_SPLIT_INDEX + 1);
        }
      }
    }
    // Transitioned to stopped. Display Reset button.
    else
    {
      strncpy(spt_rstButtonText, RESET_TEXT, sizeof(spt_rstButtonText));

      chronoHasBeenReset = false;
    }

    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }
}


// Set from time/chronometer window long DOWN click button handler.
static void tc_reset_timeout_handler(void *callback_data) {

  chronoElapsed = 0;
  text_layer_set_text(timeChronoLayer, " 0:00:00");

  strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
  text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);

  resetInProgress = false;

  // Reset splits buffer.
  splitIndex = SPLIT_INDEX_RESET;
}


// Time/chronometer window Split button. Must be displaying chrono and running.
static void tc_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "entered tc_down_single_click_handler (split button)");

  // Must be displaying chrono and running.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_START)
  {
    // If full, determine behavior based on selected setting.
    if (splitIndex == MAX_SPLIT_INDEX)
    {
      // If saving latest, throw away oldest to make room for new.
      if (splitButtonBehavior == SPLITS_KEEP_LATEST)
      {
        for (int i = 1; i <= MAX_SPLIT_INDEX; i++)
        {
          splits[i - 1] = splits [i];
        }

        splits[splitIndex] = chronoElapsed;
      }
      // else - saving oldest so throw request away this request
    }

    // Buffer is not full.
    else
    {
      splitIndex++;
      splits[splitIndex] = chronoElapsed;

      // If split buffer is now full, determine how to update splits button label.
      if (splitIndex == MAX_SPLIT_INDEX)
      {
        // If we're saving the oldest, set label to indicate splits buffer is full.
        if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s", SPLIT_TEXT_FULL);
        }

        // We're saving latest, so set label to indicate last slot number.
        else
        {
          snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 1);
        }
      }

      // Splits buffer is not full, update split button label to reflect next available slot.
      // splitIndex is set to last used, so increment by 2: 1 to make count + 1 to make next
      else
      {
        snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 2);
      }

      // Update the display.
      text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
    }
  }
}


// Time/chronometer window Reset button pressed. Must be displaying chrono, not running, and needing to be reset.
static void tc_down_down_handler(ClickRecognizerRef recognizer, Window *window) {

  // Must be displaying chrono, not running, and needing to be reset.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_STOP && ( ! chronoHasBeenReset))
  {
    resetTimerHandle = app_timer_register(1000, tc_reset_timeout_handler, NULL);

    strcpy(savedChronoText, text_layer_get_text(timeChronoLayer));
    text_layer_set_text(timeChronoLayer, "HOLD");
    savedChronoElapsed = chronoElapsed;

    resetInProgress = true;
  }
}


// Time/chronometer window Reset button released. Must be displaying chrono, not running, and needing to be reset.
static void tc_down_up_handler(ClickRecognizerRef recognizer, Window *window) {

  // Must be displaying chrono, not running, and needing to be reset.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_STOP && ( ! chronoHasBeenReset))
  {
    // Reset timeout fired, meaning we proceed with the reset.
    if (resetInProgress == false)
    {
      chronoElapsed = 0;
      text_layer_set_text(timeChronoLayer, " 0:00:00");

      strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
      text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);

      chronoHasBeenReset = true;
    }
    // Button released before reset timeout completed - abort reset!
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


// Time/chronometer click configuration.
static void tc_click_config_provider(Window *window) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "entered tc_click_config_provider");

  window_long_click_subscribe(BUTTON_ID_UP, 500, (ClickHandler) tc_up_long_click_handler, NULL);

  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) tc_select_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) tc_down_single_click_handler);
  window_raw_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) tc_down_down_handler, (ClickHandler) tc_down_up_handler, 

NULL);
}


//##################### Common support #####################################

static void app_init() {

  // ### Restore state if exists. ###
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
      strncpy(spt_rstButtonText, saved_state.spt_rstButtonText, sizeof(spt_rstButtonText));
      chronoHasBeenReset = saved_state.chronoHasBeenReset;
      for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
      {
        splits[i] = saved_state.splits[i];
      }
      splitIndex = saved_state.splitIndex;
    }
    else
    {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_read_data(). bytes read = %i", bytes_read);
    }
  }
  else
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists() returned false");
    for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
    {
      splits[i] = 0;
    }
  }

  // Font for time and chronometer.
  time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_36));

  // Get graphics.
  menuIcon = gbitmap_create_with_resource(RESOURCE_ID_MENU_IMAGE);
  light_image = gbitmap_create_with_resource(RESOURCE_ID_LIGHT_ICON);
  ss_image = gbitmap_create_with_resource(RESOURCE_ID_START_STOP);

  // ### Menu window setup. ###
  menu_window = window_create();
  Layer * menu_window_layer = window_get_root_layer(menu_window);
  window_set_fullscreen(menu_window, true);

  menuItems[0] = (SimpleMenuItem){.title = "WatchChrono",
                                  .subtitle = NULL,
                                  .callback = menuWatchChronoHandler,
                                  .icon = menuIcon};
  menuItems[1] = (SimpleMenuItem){.title = "Display Splits",
                                  .subtitle = NULL,
                                  .callback = menuDisplaySplitsHandler,
                                  .icon = NULL};
  menuItems[2] = (SimpleMenuItem){.title = "Clear Splits",
                                  .subtitle = NULL,
                                  .callback = menuClearSplitsHandler,
                                  .icon = NULL};
  menuItems[3] = (SimpleMenuItem){.title = APP_VERSION,
                                  .subtitle = NULL,
                                  .callback = NULL,
                                  .icon = NULL};

  menuSection[0] = (SimpleMenuSection){.items = menuItems, .num_items = NBR_MENU_ITEMS, .title = NULL};

  menuLayer = simple_menu_layer_create(layer_get_bounds(menu_window_layer),
                                       menu_window,
                                       menuSection,
                                       1,
                                       NULL);

  simple_menu_layer_set_selected_index(menuLayer, 0, true);
  layer_add_child(menu_window_layer, simple_menu_layer_get_layer(menuLayer));

  // ### Time/chronometer window setup ###

  // Time/chrono window setup.
  time_window = window_create();
  Layer * time_window_layer = window_get_root_layer(time_window);
  window_set_fullscreen(time_window, true);
  window_set_background_color(time_window, GColorBlack);

  // Light icon area
  lightLayer = bitmap_layer_create(GRect(84, 4, 16, 16));
  bitmap_layer_set_bitmap(lightLayer, light_image);
  layer_add_child(time_window_layer, bitmap_layer_get_layer(lightLayer));

  // Mode button
  modeButtonLayer = text_layer_create(GRect(100, 0, 44, 20));
  text_layer_set_text_alignment(modeButtonLayer, GTextAlignmentRight);
  text_layer_set_font(modeButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(modeButtonLayer, GColorBlack);
  text_layer_set_text_color(modeButtonLayer, GColorWhite);
  text_layer_set_text(modeButtonLayer, "/Mode");
  layer_add_child(time_window_layer, text_layer_get_layer(modeButtonLayer));

  // Time/chronograph area
  timeChronoLayer = text_layer_create(GRect(12, 61, 118, 38));
  text_layer_set_text_alignment(timeChronoLayer, GTextAlignmentCenter);
  text_layer_set_font(timeChronoLayer, time_font);
  text_layer_set_background_color(timeChronoLayer, GColorBlack);
  text_layer_set_text_color(timeChronoLayer, GColorWhite);
  text_layer_set_text(timeChronoLayer, timeText);
  layer_add_child(time_window_layer, text_layer_get_layer(timeChronoLayer));

  // Start/stop area
  ssLayer = bitmap_layer_create(GRect(130, 67, 14, 30));
  bitmap_layer_set_bitmap(ssLayer, ss_image);
  layer_add_child(time_window_layer, bitmap_layer_get_layer(ssLayer));
  if (selectedMode == MODE_CHRON)
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), false);
  }
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), true);
  }

  // dateStr/info area
  dateInfoLayer = text_layer_create(GRect(0, 101, 144, 20));
  text_layer_set_text_alignment(dateInfoLayer, GTextAlignmentCenter);
  text_layer_set_font(dateInfoLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(dateInfoLayer, GColorBlack);
  text_layer_set_text_color(dateInfoLayer, GColorWhite);
  text_layer_set_text(dateInfoLayer, dateStr);
  layer_add_child(time_window_layer, text_layer_get_layer(dateInfoLayer));

  // Split/Reset button
  sptRstButtonLayer = text_layer_create(GRect(0, 149, 144, 20));
  text_layer_set_text_alignment(sptRstButtonLayer, GTextAlignmentRight);
  text_layer_set_font(sptRstButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(sptRstButtonLayer, GColorBlack);
  text_layer_set_text_color(sptRstButtonLayer, GColorWhite);
  text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  layer_add_child(time_window_layer, text_layer_get_layer(sptRstButtonLayer));

  window_set_click_config_provider(time_window, (ClickConfigProvider) tc_click_config_provider);

  clock_is_24h = clock_is_24h_style();
  
  // Start keeping track of time/chrono elapsed.
  tick_timer_service_subscribe(SECOND_UNIT, tc_handle_second_tick);

  // ### Splits window setup ###

  // Create splits window.
  split_window = window_create();
  Layer * split_window_layer = window_get_root_layer(split_window);
  window_set_fullscreen(split_window, true);
  window_set_background_color(split_window, GColorWhite);

  // Title
  splitTitleLayer = text_layer_create(GRect(0, 0, 144, 28));
  text_layer_set_text_alignment(splitTitleLayer, GTextAlignmentCenter);
  text_layer_set_font(splitTitleLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(splitTitleLayer, GColorBlack);
  text_layer_set_text_color(splitTitleLayer, GColorWhite);
  text_layer_set_text(splitTitleLayer, "Splits");
  layer_add_child(split_window_layer, text_layer_get_layer(splitTitleLayer));

  splitContentLayer = text_layer_create(GRect(0, 32, 144, 136));
  text_layer_set_text_alignment(splitContentLayer, GTextAlignmentCenter);
  text_layer_set_font(splitContentLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(splitContentLayer, GColorBlack);
  text_layer_set_text_color(splitContentLayer, GColorWhite);
  text_layer_set_text(splitContentLayer, splitsDisplayContent);
  layer_add_child(split_window_layer, text_layer_get_layer(splitContentLayer));

  window_set_click_config_provider(split_window, (ClickConfigProvider) splits_click_config_provider);

  // ### Show menu window with time/chronometer immediately stacked on top it if. ###
  window_stack_push(menu_window, true /* Animated */);
  window_stack_push(time_window, true /* Animated */);
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
  strncpy(saved_state.spt_rstButtonText, spt_rstButtonText, sizeof(saved_state.spt_rstButtonText)); 
  saved_state.chronoHasBeenReset = chronoHasBeenReset;
  for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
  {
    saved_state.splits[i] = splits[i];
  }
  saved_state.splitIndex = splitIndex;

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

  // Stop reset timer if running.
  if (resetTimerHandle != NULL)
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "resetTimerHandle != NULL");
    app_timer_cancel(resetTimerHandle);
  }

  // Stop keeping track of time/chrono elapsed.
  tick_timer_service_unsubscribe();

  // Destroy splits window.
  text_layer_destroy(splitContentLayer);
  text_layer_destroy(splitSepLayer);
  text_layer_destroy(splitTitleLayer);
  window_destroy(split_window);

  // Destroy time/chrono window.
  bitmap_layer_destroy(lightLayer);
  text_layer_destroy(modeButtonLayer);
  text_layer_destroy(timeChronoLayer);
  text_layer_destroy(dateInfoLayer);
  bitmap_layer_destroy(ssLayer);
  text_layer_destroy(sptRstButtonLayer);
  window_destroy(time_window);

  // Destroy menu window.
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "before menu destroy");
  gbitmap_destroy(light_image);
  gbitmap_destroy(menuIcon);
  gbitmap_destroy(ss_image);
  simple_menu_layer_destroy(menuLayer);
  window_destroy(menu_window);
}


int main(void) {
  app_init();
  app_event_loop();
  app_deinit();
}