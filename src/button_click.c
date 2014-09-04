// WatchChronometer (c) 2014 Keith Blom - All rights reserved
// v2.4 - Abbreviate month.
// v2.3 - Added day of week to watch display.
// v2.2 - Rework time/chrono fonts/format for larger display. Add color inversion option.
// v2.1.1 - Arrows on splits display.
// v2.1 - Add option selection.
// v2.0 - Add splits.

#define APP_VERSION "Ver 2.4"

// Standard includes
#include "pebble.h"

// Forward declarations.
void format_splits_content();
void setup_splits_window();
void select_splits_display_content();

// Menu window is pushed to the stack first, then the time window below it.
static Window *option_window; 
static Window *menu_window; 
static Window *split_window; 
static Window *time_window; 

// Menu window layers and associated data.
#define NBR_MENU_ITEMS 7
static SimpleMenuLayer *menuLayer;
static SimpleMenuItem menuItems[NBR_MENU_ITEMS];
static SimpleMenuSection menuSection[1];

// Time/chronometer window layers.
static BitmapLayer *lightLayer;
static TextLayer *modeButtonLayer;
static TextLayer *timeChronoHhmmLayer;
static TextLayer *timeChronoSecLayer;
static BitmapLayer *ssLayer;
static TextLayer *dateInfoLayer;
static TextLayer *sptRstButtonLayer;
static InverterLayer *tcInverterLayer = 0;

// Splits window layers.
static TextLayer *splitTitleLayer;
static BitmapLayer *splitUpIconLayer;
static TextLayer *splitContentLayer;
static BitmapLayer *splitDnIconLayer;

// Option window layers.
static TextLayer *optionUpLabelLayer;
static TextLayer *optionContentLayer;
static TextLayer *optionDownLabelLayer;

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

// Fonts for time and chronometer.
GFont hhmm_font;
GFont sec_font;

// Graphics
GBitmap* menuIcon;
GBitmap *light_image;
GBitmap *ss_image;
GBitmap *up_image;
GBitmap *dn_image;

// Time chronometer was started, or zero if reset.
static time_t chronoElapsed = 0;

// Time or chronograph value as text string.
#define MAX_TIME_TEXT_LEN 9
static char timeText[] = "00:00:00";

// Month day or "CHRONO".
// 2.1.1 static char dateStr[] = "Jan 31"; 
static char dateStr[] = "Thursday/nSeptember 30"; 

// Saved chrono time during wait for reset.
static char savedChronoHhmm[] = "00:00";
static char savedChronoSec[] = "00"; 
static time_t savedChronoElapsed = 0;
static bool resetInProgress = false;
static AppTimer *resetTimerHandle = NULL;

// 12/24 hour clock. Access once and remember.
static bool clock_is_24h = false;

// "true" if the chronometer has been reset after being stopped.
// (i.e. do not need to display RESET text on a stopped and reset chronometer)
static bool chronoHasBeenReset = true;

// Split/reset button text.
#define SPLIT_TEXT_MAX_LEN 11
static char BLANK_TEXT[] = "";
static char RESET_TEXT[] = "Reset";
static char SPLIT_TEXT[] = "Split";     // Will be appended with next available split number.
static char SPLIT_TEXT_FULL[] = "Split Full";
static char spt_rstButtonText[SPLIT_TEXT_MAX_LEN] = ""; // Space for "Split Full" w/ null terminator

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

// Support for option window.
#define OPTION_CHOICE_YES "Yes"
#define OPTION_CHOICE_NO "No"
#define OPTION_CHOICE_MAX_LEN 4
#define OPTION_TEXT_MAX_LEN 256
static char optionText[OPTION_TEXT_MAX_LEN]; // Made global as a convenience, not required.

// Support for Clear Splits.
static char clearSplitsText[] = "Select 'Yes' to clear splits.";
static char splitsClearedText[] = "Splits have been cleared.";
static char noSplitsText[] = "There are no splits.";

// Support for Reset behavior option.
static char resetOptionText[] = "Chronometer Reset button also clears splits:";
static char resetButtonClearsSplits[OPTION_CHOICE_MAX_LEN] = OPTION_CHOICE_YES;

// Support for Split behavior option.
static char splitsOptionText[] = "When splits memory is Full, replace oldest with new:";
static char splitsFullReplaceOldest[OPTION_CHOICE_MAX_LEN] = OPTION_CHOICE_NO;

// Support for Color option.
static char colorOptionText[] = "Display time and chrono with white text on black background:";
static char colorOptionChoice[OPTION_CHOICE_MAX_LEN] = OPTION_CHOICE_NO;

// Structure to save state when app is not running.
typedef struct saved_state_S
{
  short selectedMode;
  char timeText [MAX_TIME_TEXT_LEN];
  char dateStr [7];
  short chronoRunSelect;
  time_t chronoElapsed;
  time_t closeTm;
  char spt_rstButtonText[SPLIT_TEXT_MAX_LEN];
  bool chronoHasBeenReset;
  //time_t unused;
  char colorOptionChoice[OPTION_CHOICE_MAX_LEN];
  time_t splits[MAX_SPLIT_INDEX + 1];
  int splitIndex;
  char resetButtonClearsSplits[OPTION_CHOICE_MAX_LEN];
  char splitsFullReplaceOldest[OPTION_CHOICE_MAX_LEN];
} __attribute__((__packed__)) saved_state_S;




//##################### Option window support ################################

// ### Clear splits support ###

// Clear splits UP button - do it!
static void clear_splits_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  strcpy(formattedSplits, SPLITS_DISPLAY_NONE);

  // Clear splits.
  splitIndex = -1;
  splitDisplayIndex = 0;
  select_splits_display_content();

  // If the chronometer is running and was the last thing displayed before
  // navigation to Clear splits, need to label Splits button for split 1.
  if (selectedMode == MODE_CHRON && chronoRunSelect == RUN_START)
  {
    snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, splitIndex + 2);
  }

  layer_set_hidden(bitmap_layer_get_layer(splitUpIconLayer), true);
  layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), true);

  // Update option window to reflect action.
  text_layer_set_text(optionContentLayer, splitsClearedText);
}


// Clear splits click configuration.
static void clear_splits_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) clear_splits_up_single_click_handler);
}

// ### Splits option support ###

// Splits option UP button - save latest.
static void splits_option_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice for Split button use.
  strncpy(splitsFullReplaceOldest, OPTION_CHOICE_YES, sizeof(splitsFullReplaceOldest));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", splitsOptionText, splitsFullReplaceOldest);
  text_layer_set_text(optionContentLayer, optionText);

  // If split/reset button currently indicates Full, change to last split slot number.
  if (strcmp(spt_rstButtonText, SPLIT_TEXT_FULL) == 0)
  {
    snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s %i", SPLIT_TEXT, MAX_SPLIT_INDEX + 1);
    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }
}


// Splits option DOWN button - save oldest.
static void splits_option_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice for Split button use.
  strncpy(splitsFullReplaceOldest, OPTION_CHOICE_NO, sizeof(splitsFullReplaceOldest));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", splitsOptionText, splitsFullReplaceOldest);
  text_layer_set_text(optionContentLayer, optionText);

  // If split/reset button currently indicates last split slot number, change to indicate Full.
  char testStr[SPLIT_TEXT_MAX_LEN];
  snprintf(testStr, SPLIT_TEXT_MAX_LEN, "%s %i", SPLIT_TEXT, MAX_SPLIT_INDEX + 1);
  if (strcmp(spt_rstButtonText, testStr) == 0)
  {
    snprintf(spt_rstButtonText, sizeof(spt_rstButtonText), "%s", SPLIT_TEXT_FULL);
    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }
}


// Splits option click configuration.
static void splits_option_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) splits_option_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) splits_option_down_single_click_handler);
}

// ### Reset option support ###

// Reset option UP button.
static void reset_option_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice for Reset button use.
  strncpy(resetButtonClearsSplits, OPTION_CHOICE_YES, sizeof(resetButtonClearsSplits));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", resetOptionText, resetButtonClearsSplits);
  text_layer_set_text(optionContentLayer, optionText);
}


// Reset option DOWN button.
static void reset_option_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice for Reset button use.
  strncpy(resetButtonClearsSplits, OPTION_CHOICE_NO, sizeof(resetButtonClearsSplits));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", resetOptionText, resetButtonClearsSplits);
  text_layer_set_text(optionContentLayer, optionText);
}


// Reset option click configuration.
static void reset_option_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) reset_option_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) reset_option_down_single_click_handler);
}

// ### Color option support ###

// Color option UP button.
static void Color_option_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice.
  strncpy(colorOptionChoice, OPTION_CHOICE_YES, sizeof(colorOptionChoice));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorOptionText, colorOptionChoice);
  text_layer_set_text(optionContentLayer, optionText);

  // Change the time/chrono window to reflect the choice.
  layer_set_hidden(inverter_layer_get_layer(tcInverterLayer), false);
}


// Color option DOWN button.
static void Color_option_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice.
  strncpy(colorOptionChoice, OPTION_CHOICE_NO, sizeof(colorOptionChoice));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorOptionText, colorOptionChoice);
  text_layer_set_text(optionContentLayer, optionText);

  // Change the time/chrono window to reflect the choice.
  layer_set_hidden(inverter_layer_get_layer(tcInverterLayer), true);
}


// Color option click configuration.
static void Color_option_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) Color_option_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) Color_option_down_single_click_handler);
}

//##################### Menu window support ################################

void menuAppearHandler(struct Window *window) {

  layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), false);
  layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), false);
}


static void menuWatchChronoHandler(int index, void *context)
{
  window_stack_push(time_window, true /* Animated */);
}


static void menuDisplaySplitsHandler(int index, void *context)
{
  format_splits_content();

  splitDisplayIndex = 0;
  select_splits_display_content();

  layer_set_hidden(bitmap_layer_get_layer(splitUpIconLayer), true);

  if (splitIndex >= MAX_DISPLAY_SPLITS)
  {
    layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), false);
  }
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), true);
  }

  text_layer_set_text(splitContentLayer, splitsDisplayContent);
  window_stack_push(split_window, true /* Animated */);
}


static void menuClearSplitsHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) clear_splits_click_config_provider);

  if (splitIndex > 0)
  {
    layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), true);
    text_layer_set_text(optionContentLayer, clearSplitsText);
  }
  else
  {
    layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), true);
    layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), true);
    text_layer_set_text(optionContentLayer, noSplitsText);
  }

  window_stack_push(option_window, true /* Animated */);
}


static void menuSplitsOptionHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) splits_option_click_config_provider);

  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", splitsOptionText, splitsFullReplaceOldest);
  text_layer_set_text(optionContentLayer, optionText);
  window_stack_push(option_window, true /* Animated */);
}


static void menuResetOptionHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) reset_option_click_config_provider);

  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", resetOptionText, resetButtonClearsSplits);
  text_layer_set_text(optionContentLayer, optionText);
  window_stack_push(option_window, true /* Animated */);
}


static void menuColorOptionHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) Color_option_click_config_provider);

  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorOptionText, colorOptionChoice);
  text_layer_set_text(optionContentLayer, optionText);
  window_stack_push(option_window, true /* Animated */);
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

    // If page going to is not first, show UP icon. KAB
    if (splitDisplayIndex > 0)
    {
      layer_set_hidden(bitmap_layer_get_layer(splitUpIconLayer), false);
    }
    else
    {
      layer_set_hidden(bitmap_layer_get_layer(splitUpIconLayer), true);
    }

    // Always need a DOWN icon.
    layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), false);

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

    // Always need an UP icon.
    layer_set_hidden(bitmap_layer_get_layer(splitUpIconLayer), false);

    // If page going to is not last, show DOWN icon.
    if (splitDisplayIndex + MAX_DISPLAY_SPLITS <= splitIndex)
    {
      layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), false);
    }
    else
    {
      layer_set_hidden(bitmap_layer_get_layer(splitDnIconLayer), true);
    }

  }
}


// Splits click configuration.
static void splits_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) splits_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) splits_down_single_click_handler);
}

//##################### Time/chrono window support ##########################

// 
static void tc_set_tc_layer_text() 
{
  // Remove ":ss" and set hours/minutes with remainder.
  static char tempHhmm [MAX_TIME_TEXT_LEN];
  int hhMmLen = strlen(timeText) - 3;
  strncpy(tempHhmm, timeText, hhMmLen); 
  tempHhmm[hhMmLen] = '\0';
  text_layer_set_text(timeChronoHhmmLayer, tempHhmm);

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "len: %i, HHMM >%s<", hhMmLen, tempHhmm);

  // Set seconds.
  char * secStartPtr = &(timeText[hhMmLen + 1]);
  text_layer_set_text(timeChronoSecLayer, secStartPtr);

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SS >%s<", &(timeText[hhMmLen + 1]));
}

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

    //text_layer_set_text(timeChronoHhmmLayer, timeText);
    tc_set_tc_layer_text();
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

    //text_layer_set_text(timeChronoHhmmLayer, timeText);
    tc_set_tc_layer_text();

    //2.1.1 strftime(dateStr, 7, "%b", currentTime);
    //2.1.1 snprintf(&(dateStr[3]), 4, " %i",  currentTime->tm_mday);
    strftime(dateStr, 22, "%A%n%b %d", currentTime);
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
        //if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        if (strcmp(splitsFullReplaceOldest, OPTION_CHOICE_NO) == 0)
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
        //if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        if (strcmp(splitsFullReplaceOldest, OPTION_CHOICE_NO) == 0)
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
  text_layer_set_text(timeChronoHhmmLayer, " 0:00");
  text_layer_set_text(timeChronoSecLayer, "00");

  strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
  text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);

  resetInProgress = false;

  // Reset splits buffer if the option is active.
  if (strcmp(resetButtonClearsSplits, OPTION_CHOICE_YES) == 0)
  {
    splitIndex = SPLIT_INDEX_RESET;
  }
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
      //if (splitButtonBehavior == SPLITS_KEEP_LATEST)
      if (strcmp(splitsFullReplaceOldest, OPTION_CHOICE_YES) == 0)
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
        // If we're saving the oldest, set label to indicate splits buffer is now full.
        //if (splitButtonBehavior == SPLITS_KEEP_OLDEST)
        if (strcmp(splitsFullReplaceOldest, OPTION_CHOICE_NO) == 0)
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

    strcpy(savedChronoHhmm, text_layer_get_text(timeChronoHhmmLayer));
    strcpy(savedChronoSec, text_layer_get_text(timeChronoSecLayer));
    text_layer_set_text(timeChronoHhmmLayer, "HOLD");
    text_layer_set_text(timeChronoSecLayer, "");
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
      //chronoElapsed = 0;
      //text_layer_set_text(timeChronoHhmmLayer, " 0:00:00");

      //strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
      //text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);

      chronoHasBeenReset = true;
    }
    // Button released before reset timeout completed - abort reset!
    else
    {
      text_layer_set_text(timeChronoHhmmLayer, savedChronoHhmm);
      text_layer_set_text(timeChronoSecLayer, savedChronoSec);
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
      strncpy(resetButtonClearsSplits, saved_state.resetButtonClearsSplits, sizeof(resetButtonClearsSplits));
      strncpy(splitsFullReplaceOldest, saved_state.splitsFullReplaceOldest, sizeof(splitsFullReplaceOldest));
      strncpy(colorOptionChoice, saved_state.colorOptionChoice, sizeof(colorOptionChoice));
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

  // Fonts for time and chronometer.
  hhmm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_46));
  sec_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_24));

  // Get graphics.
  menuIcon = gbitmap_create_with_resource(RESOURCE_ID_MENU_IMAGE);
  light_image = gbitmap_create_with_resource(RESOURCE_ID_LIGHT_ICON);
  ss_image = gbitmap_create_with_resource(RESOURCE_ID_START_STOP);
  up_image = gbitmap_create_with_resource(RESOURCE_ID_UP_ICON);
  dn_image = gbitmap_create_with_resource(RESOURCE_ID_DN_ICON);

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
  menuItems[3] = (SimpleMenuItem){.title = "Splits Option",
                                  .subtitle = NULL,
                                  .callback = menuSplitsOptionHandler,
                                  .icon = NULL};
  menuItems[4] = (SimpleMenuItem){.title = "Reset Option",
                                  .subtitle = NULL,
                                  .callback = menuResetOptionHandler,
                                  .icon = NULL};
  menuItems[5] = (SimpleMenuItem){.title = "Color Option",
                                  .subtitle = NULL,
                                  .callback = menuColorOptionHandler,
                                  .icon = NULL};
  menuItems[6] = (SimpleMenuItem){.title = APP_VERSION,
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
  window_set_background_color(time_window, GColorWhite);

  // Light icon area
  lightLayer = bitmap_layer_create(GRect(82, 4, 16, 16));
  bitmap_layer_set_bitmap(lightLayer, light_image);
  layer_add_child(time_window_layer, bitmap_layer_get_layer(lightLayer));

  // Mode button
  modeButtonLayer = text_layer_create(GRect(98, 0, 44, 20));
  text_layer_set_text_alignment(modeButtonLayer, GTextAlignmentRight);
  text_layer_set_font(modeButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(modeButtonLayer, GColorWhite);
  text_layer_set_text_color(modeButtonLayer, GColorBlack);
  text_layer_set_text(modeButtonLayer, "/Mode");
  layer_add_child(time_window_layer, text_layer_get_layer(modeButtonLayer));

  // Time/chronograph area - Hours & Minutes
  // 2.1.1 timeChronoHhmmLayer = text_layer_create(GRect(0, 55, 102, 46));
  timeChronoHhmmLayer = text_layer_create(GRect(0, 48, 102, 46));
  text_layer_set_text_alignment(timeChronoHhmmLayer, GTextAlignmentRight);
  text_layer_set_font(timeChronoHhmmLayer, hhmm_font);
  text_layer_set_background_color(timeChronoHhmmLayer, GColorWhite);
  text_layer_set_text_color(timeChronoHhmmLayer, GColorBlack);

  // Time/chronograph area - Seconds
  // 2.1.1 timeChronoSecLayer = text_layer_create(GRect(104, 69, 26, 26));
  timeChronoSecLayer = text_layer_create(GRect(104, 64, 26, 26));
  text_layer_set_text_alignment(timeChronoSecLayer, GTextAlignmentLeft);
  text_layer_set_font(timeChronoSecLayer, sec_font);
  text_layer_set_background_color(timeChronoSecLayer, GColorWhite);
  text_layer_set_text_color(timeChronoSecLayer, GColorBlack);

  // Time/chronograph area - common
  //text_layer_set_text(timeChronoHhmmLayer, timeText);
  tc_set_tc_layer_text();
  layer_add_child(time_window_layer, text_layer_get_layer(timeChronoHhmmLayer));
  layer_add_child(time_window_layer, text_layer_get_layer(timeChronoSecLayer));

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
  //2.1.1 dateInfoLayer = text_layer_create(GRect(0, 105, 144, 28));
  dateInfoLayer = text_layer_create(GRect(10, 94, 120, 52));
  text_layer_set_text_alignment(dateInfoLayer, GTextAlignmentCenter);
  //text_layer_set_font(dateInfoLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_font(dateInfoLayer, sec_font);
  text_layer_set_background_color(dateInfoLayer, GColorWhite);
  text_layer_set_text_color(dateInfoLayer, GColorBlack);
  text_layer_set_text(dateInfoLayer, dateStr);
  layer_add_child(time_window_layer, text_layer_get_layer(dateInfoLayer));

  // Split/Reset button
  sptRstButtonLayer = text_layer_create(GRect(0, 146, 142, 20));
  text_layer_set_text_alignment(sptRstButtonLayer, GTextAlignmentRight);
  text_layer_set_font(sptRstButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_background_color(sptRstButtonLayer, GColorWhite);
  text_layer_set_text_color(sptRstButtonLayer, GColorBlack);
  text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  layer_add_child(time_window_layer, text_layer_get_layer(sptRstButtonLayer));

  // Color inversion option.
  tcInverterLayer = inverter_layer_create(GRect(0, 0, 144, 168));
  layer_add_child(time_window_layer, inverter_layer_get_layer(tcInverterLayer));
  if (strcmp(colorOptionChoice, OPTION_CHOICE_NO) == 0)
  {
    layer_set_hidden(inverter_layer_get_layer(tcInverterLayer), true);
  }
  else
  {
    layer_set_hidden(inverter_layer_get_layer(tcInverterLayer), false);
  }

  window_set_click_config_provider(time_window, (ClickConfigProvider) tc_click_config_provider);

  clock_is_24h = clock_is_24h_style();
  
  // Start keeping track of time/chrono elapsed.
  tick_timer_service_subscribe(SECOND_UNIT, tc_handle_second_tick);

  // ### Splits window setup ###

  // Create splits window.
  split_window = window_create();
  Layer * split_window_layer = window_get_root_layer(split_window);
  window_set_fullscreen(split_window, true);
  window_set_background_color(split_window, GColorBlack);

  // Title
  splitTitleLayer = text_layer_create(GRect(0, 0, 144, 28));
  text_layer_set_text_alignment(splitTitleLayer, GTextAlignmentCenter);
  text_layer_set_font(splitTitleLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(splitTitleLayer, GColorWhite);
  text_layer_set_text_color(splitTitleLayer, GColorBlack);
  text_layer_set_text(splitTitleLayer, "Splits");
  layer_add_child(split_window_layer, text_layer_get_layer(splitTitleLayer));

  // UP icon
  splitUpIconLayer = bitmap_layer_create(GRect(129, 4, 15, 15));
  bitmap_layer_set_bitmap(splitUpIconLayer, up_image);
  layer_add_child(split_window_layer, bitmap_layer_get_layer(splitUpIconLayer));

  // Splits content
  splitContentLayer = text_layer_create(GRect(0, 32, 144, 136));
  text_layer_set_text_alignment(splitContentLayer, GTextAlignmentCenter);
  text_layer_set_font(splitContentLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(splitContentLayer, GColorWhite);
  text_layer_set_text_color(splitContentLayer, GColorBlack);
  text_layer_set_text(splitContentLayer, splitsDisplayContent);
  layer_add_child(split_window_layer, text_layer_get_layer(splitContentLayer));

  // DOWN icon
  splitDnIconLayer = bitmap_layer_create(GRect(129, 157, 15, 15));
  bitmap_layer_set_bitmap(splitDnIconLayer, dn_image);
  layer_add_child(split_window_layer, bitmap_layer_get_layer(splitDnIconLayer));

  window_set_click_config_provider(split_window, (ClickConfigProvider) splits_click_config_provider);

  // ### Option window setup ###

  // Create option window.
  option_window = window_create();
  Layer * option_window_layer = window_get_root_layer(option_window);
  window_set_fullscreen(option_window, true);
  window_set_background_color(option_window, GColorWhite);

  // Up button label - "Yes"
  optionUpLabelLayer = text_layer_create(GRect(0, 0, 142, 28));
  text_layer_set_text_alignment(optionUpLabelLayer, GTextAlignmentRight);
  text_layer_set_font(optionUpLabelLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(optionUpLabelLayer, GColorWhite);
  text_layer_set_text_color(optionUpLabelLayer, GColorBlack);
  text_layer_set_text(optionUpLabelLayer, OPTION_CHOICE_YES);
  layer_add_child(option_window_layer, text_layer_get_layer(optionUpLabelLayer));

  // Option content
  optionContentLayer = text_layer_create(GRect(4, 32, 136, 136));
  text_layer_set_text_alignment(optionContentLayer, GTextAlignmentCenter);
  text_layer_set_font(optionContentLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(optionContentLayer, GColorWhite);
  text_layer_set_text_color(optionContentLayer, GColorBlack);
  text_layer_set_text(optionContentLayer, "test");
  layer_add_child(option_window_layer, text_layer_get_layer(optionContentLayer));

  // Down button label - "No"
  optionDownLabelLayer = text_layer_create(GRect(0, 141, 142, 26));
  text_layer_set_text_alignment(optionDownLabelLayer, GTextAlignmentRight);
  text_layer_set_font(optionDownLabelLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(optionDownLabelLayer, GColorWhite);
  text_layer_set_text_color(optionDownLabelLayer, GColorBlack);
  text_layer_set_text(optionDownLabelLayer, OPTION_CHOICE_NO);
  layer_add_child(option_window_layer, text_layer_get_layer(optionDownLabelLayer));

  // Note: window_set_click_config_provider() for option window will be set based on option selected.

  // Hook to restore button labels, etc, that may need to be modified by some menu items.
  window_set_window_handlers(menu_window, (WindowHandlers){.appear = menuAppearHandler});

  // ### Show menu window with time/chronometer immediately stacked on top of it. ###
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
  strncpy(saved_state.resetButtonClearsSplits, resetButtonClearsSplits, sizeof(saved_state.resetButtonClearsSplits));
  strncpy(saved_state.splitsFullReplaceOldest, splitsFullReplaceOldest, sizeof(saved_state.splitsFullReplaceOldest));
  strncpy(saved_state.colorOptionChoice, colorOptionChoice, sizeof(saved_state.colorOptionChoice));

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

  // Destroy option window.
  text_layer_destroy(optionContentLayer);
  text_layer_destroy(optionUpLabelLayer);
  text_layer_destroy(optionDownLabelLayer);
  window_destroy(option_window);

  // Destroy splits window.
  bitmap_layer_destroy(splitDnIconLayer);
  text_layer_destroy(splitContentLayer);
  bitmap_layer_destroy(splitUpIconLayer);
  text_layer_destroy(splitTitleLayer);
  window_destroy(split_window);

  // Destroy time/chrono window.
  if (tcInverterLayer != 0)
  {
    inverter_layer_destroy(tcInverterLayer);
    tcInverterLayer = 0;
  }
  bitmap_layer_destroy(lightLayer);
  text_layer_destroy(modeButtonLayer);
  text_layer_destroy(timeChronoSecLayer);
  text_layer_destroy(timeChronoHhmmLayer);
  text_layer_destroy(dateInfoLayer);
  bitmap_layer_destroy(ssLayer);
  text_layer_destroy(sptRstButtonLayer);
  window_destroy(time_window);

  // Destroy menu window.
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "before menu destroy");
  gbitmap_destroy(light_image);
  gbitmap_destroy(menuIcon);
  gbitmap_destroy(ss_image);
  gbitmap_destroy(up_image);
  gbitmap_destroy(dn_image);
  simple_menu_layer_destroy(menuLayer);
  window_destroy(menu_window);
}


int main(void) {
  app_init();
  app_event_loop();
  app_deinit();
}