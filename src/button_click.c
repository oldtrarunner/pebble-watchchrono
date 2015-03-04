// WatchChronometer (c) 2014 Keith Blom - All rights reserved
// v4.0 - Support SDK v3.0. Replace black wth color select.
// v3.0 - Moved options to Down button on Watch display.
// v2.5 - Support 99 splits
// v2.4.1 - Remove leading zero on date.
// v2.4 - Abbreviate month.
// v2.3 - Added day of week to watch display.
// v2.2 - Rework time/chrono fonts/format for larger display. Add color inversion option.
// v2.1.1 - Arrows on splits display.
// v2.1 - Add option selection.
// v2.0 - Add splits.

#define APP_VERSION "Ver 4.0"

// Standard includes
#include "pebble.h"

// Forward declarations.
void format_splits_content();
void setup_splits_window();
void select_splits_display_content();
static void tc_set_color();

// Menu window is pushed to the stack first, then the time window below it.
static Window *option_window; 
static Window *menu_window; 
static Window *split_window; 
static Window *time_window; 

// Menu window layers and associated data.
#ifdef PBL_COLOR
#define NBR_MENU_ITEMS 7
#else
#define NBR_MENU_ITEMS 6
#endif
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
//static InverterLayer *tcInverterLayer = 0;

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
GBitmap *up_image;
GBitmap *dn_image;
static GPath *startIconP = NULL;
static GPath *stopIconP = NULL;
static const GPathInfo STOP_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint []) {{0, 8}, {13, 8}, {13, 21}, {0, 21}}
};
static const GPathInfo START_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint []) {{0, 8}, {12, 15}, {0, 22}}
};

// SDK 3.0 support for color option
GColor colorDark;

// Time chronometer was started, or zero if reset.
static time_t chronoElapsed = 0;

// Time or chronograph value as text string.
#define MAX_TIME_TEXT_LEN 9
static char timeText[] = "00:00:00";

// Month day or "CHRONO".
// 2.1.1 static char dateStr[] = "Jan 31"; 
static char dateStr[] = "Wednesday/nSep 30"; 

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
static char OPTIONS_TEXT[] = "Options";
static char RESET_TEXT[] = "Reset";
static char SPLIT_TEXT[] = "Split";     // Will be appended with next available split number.
static char SPLIT_TEXT_FULL[] = "Split Full";
static char spt_rstButtonText[SPLIT_TEXT_MAX_LEN] = ""; // Space for "Split Full" w/ null terminator

// Splits. Earliest first. Split format " 1) 12:34:56" plus newline/null.
#define SPLIT_INDEX_RESET -1
#define MAX_SPLIT_INDEX 98
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

// Support for Color inversion.
static char colorInversionText[] = "Display time and chrono with white text on dark background:";
static char colorInversionChoice[OPTION_CHOICE_MAX_LEN] = OPTION_CHOICE_NO;

// Support for Color selection.
#ifdef PBL_COLOR
#define MIN_COLOR_SELECTION_OFFSET 0
#define MAX_COLOR_SELECTION_OFFSET 14
static int colorSelectChoice = MIN_COLOR_SELECTION_OFFSET;
static char * colorSelectText[MAX_COLOR_SELECTION_OFFSET + 1] = 
  {"Red",
   "Light Red",
   "Dark Red",
   "Orange",
   "Light Orange",
   "Dark Orange",
   "Blue",
   "Light Blue",
   "Dark Blue",
   "Green",
   "Light Green",
   "Dark Green",
   "Purple",
   "Light Purple",
   "Dark Purple"};
static GColor colorSelectColor[MAX_COLOR_SELECTION_OFFSET + 1];
#endif

// Keys to access persistent data.
static const uint32_t  persistent_data_key = 1;
static const uint32_t  extended_splits_key = 2;

// Divide splits between two sets of persistent data so do not exceed 256 byte max size.
// Sum of BASE and EXTENDED must equal (MAX_SPLIT_INDEX + 1).
#define BASE_SPLIT_CNT 46
#define EXTENDED_SPLIT_CNT 53

// Structure to save state when app is not running.
typedef struct saved_state_S
{
  short selectedMode;
  char timeText [MAX_TIME_TEXT_LEN];
  char dateStr [17];
  short chronoRunSelect;
  time_t chronoElapsed;
  time_t closeTm;
  char spt_rstButtonText[SPLIT_TEXT_MAX_LEN];
  bool chronoHasBeenReset;
  char colorInversionChoice[OPTION_CHOICE_MAX_LEN];
  time_t splits[BASE_SPLIT_CNT];
  int splitIndex;
  char resetButtonClearsSplits[OPTION_CHOICE_MAX_LEN];
  char splitsFullReplaceOldest[OPTION_CHOICE_MAX_LEN];
  #ifdef PBL_COLOR
  int colorSelectChoice;
  #endif
} __attribute__((__packed__)) saved_state_S;


// Structure to save extended splits
typedef struct saved_splits_S
{
  time_t splits[EXTENDED_SPLIT_CNT];
} __attribute__((__packed__)) saved_splits_S;




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

// ### Color inversion support ###

// Color inversion UP button.
static void color_inversion_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice.
  strncpy(colorInversionChoice, OPTION_CHOICE_YES, sizeof(colorInversionChoice));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorInversionText, colorInversionChoice);
  text_layer_set_text(optionContentLayer, optionText);

  // tc_set_color() will be called by the time/chrono .appear window handler.
}


// Color inversion DOWN button.
static void color_inversion_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  // Save choice.
  strncpy(colorInversionChoice, OPTION_CHOICE_NO, sizeof(colorInversionChoice));

  // Update option window to reflect choice.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorInversionText, colorInversionChoice);
  text_layer_set_text(optionContentLayer, optionText);

  // tc_set_color() will be called by the time/chrono .appear window handler.
}


// Color inversion click configuration.
static void color_inversion_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) color_inversion_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) color_inversion_down_single_click_handler);
}


// ### Color select support ###

#ifdef PBL_COLOR
static void color_select_set_choice()
{
  // Adjust DOWN/UP button text.
  if (colorSelectChoice >= MAX_COLOR_SELECTION_OFFSET)
  {
    //layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), false);
    //layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), true);
    text_layer_set_text(optionUpLabelLayer, "Prev");
    text_layer_set_text(optionDownLabelLayer, "");
  }
  else if (colorSelectChoice <= MIN_COLOR_SELECTION_OFFSET)
  {
    //layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), true);
    //layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), false);
    text_layer_set_text(optionUpLabelLayer, "");
    text_layer_set_text(optionDownLabelLayer, "Next");
  }
  else
  {
    //layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), false);
    //layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), false);
    text_layer_set_text(optionUpLabelLayer, "Prev");
    text_layer_set_text(optionDownLabelLayer, "Next");
  }

  // Set current color text.
  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s", colorSelectText[colorSelectChoice]);
  text_layer_set_text(optionContentLayer, optionText);

  // Set current color as foreground.
  text_layer_set_text_color(optionContentLayer, colorSelectColor[colorSelectChoice]);
}


// Color select UP button.
static void color_select_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  if (colorSelectChoice > MIN_COLOR_SELECTION_OFFSET)
  {
    // Save choice.
    colorSelectChoice--;

    // Update option window to reflect choice.
    color_select_set_choice();
  }
}


// Color select DOWN button.
static void color_select_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {

  if (colorSelectChoice < MAX_COLOR_SELECTION_OFFSET)
  {
    // Save choice.
    colorSelectChoice++;

    // Update option window to reflect choice.
    color_select_set_choice();
  }
}


// Color select click configuration.
static void color_select_click_config_provider(Window *window) {

  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) color_select_up_single_click_handler);

  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) color_select_down_single_click_handler);
}
#endif


//##################### Menu window support ################################

void menuAppearHandler(struct Window *window) {

  layer_set_hidden(text_layer_get_layer(optionUpLabelLayer), false);
  text_layer_set_text(optionUpLabelLayer, "Yes");

  layer_set_hidden(text_layer_get_layer(optionDownLabelLayer), false);
  text_layer_set_text(optionDownLabelLayer, "No");
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


static void menuColorInversionHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) color_inversion_click_config_provider);

  snprintf(optionText, OPTION_TEXT_MAX_LEN, "%s %s", colorInversionText, colorInversionChoice);
  text_layer_set_text(optionContentLayer, optionText);
  window_stack_push(option_window, true /* Animated */);
}


#ifdef PBL_COLOR
static void menuColorSelectHandler(int index, void *context)
{
  window_set_click_config_provider(option_window, (ClickConfigProvider) color_select_click_config_provider);

  color_select_set_choice();

  window_stack_push(option_window, true /* Animated */);
}
#endif


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

void timeAppearHandler(struct Window *window) {

  tc_set_color();
}


void tc_lightLayer_update_proc (Layer *my_layer, GContext* ctx)
{
  // Dark foreground on white background.
  if (strcmp(colorInversionChoice, OPTION_CHOICE_NO) == 0)
  {
    graphics_context_set_stroke_color(ctx, colorDark);
  }

  // White foreground on dark background.
  else
  {
    graphics_context_set_stroke_color(ctx, GColorWhite);
  }

  graphics_draw_circle(ctx, GPoint(7, 7), 3);

  // East West
  graphics_draw_line(ctx, GPoint(2, 7), GPoint(3, 7));
  graphics_draw_line(ctx, GPoint(11, 7), GPoint(12, 7));

  // Northwest Southeast
  graphics_draw_line(ctx, GPoint(3, 3), GPoint(4, 4));
  graphics_draw_line(ctx, GPoint(10, 10), GPoint(11, 11));

  // North South
  graphics_draw_line(ctx, GPoint(7, 2), GPoint(7, 3));
  graphics_draw_line(ctx, GPoint(7, 11), GPoint(7, 12));

  // Northeast Southwest
  graphics_draw_line(ctx, GPoint(11, 3), GPoint(10, 4));
  graphics_draw_line(ctx, GPoint(3, 11), GPoint(4, 10));
}


void tc_ssLayer_update_proc (Layer *my_layer, GContext* ctx)
{
  // Dark foreground on white background.
  if (strcmp(colorInversionChoice, OPTION_CHOICE_NO) == 0)
  {
    graphics_context_set_fill_color(ctx, colorDark);
  }

  // White foreground on dark background.
  else
  {
    graphics_context_set_fill_color(ctx, GColorWhite);
  }

  // Set STOP icon when running.
  if (chronoRunSelect == RUN_START)
  {
    gpath_draw_filled(ctx, stopIconP);
  }

  // Set START icon when stopped.
  else
  {
    gpath_draw_filled(ctx, startIconP);
  }
}


static void tc_set_color()
{
  #ifdef PBL_COLOR
  colorDark = colorSelectColor[colorSelectChoice];
  #else
  colorDark = GColorBlack;
  #endif

  // Dark foreground on white background.
  if (strcmp(colorInversionChoice, OPTION_CHOICE_NO) == 0)
  {
    window_set_background_color(time_window, GColorWhite);
    text_layer_set_background_color(modeButtonLayer, GColorWhite);
    text_layer_set_background_color(timeChronoHhmmLayer, GColorWhite);
    text_layer_set_background_color(timeChronoSecLayer, GColorWhite);
    text_layer_set_background_color(dateInfoLayer, GColorWhite);
    text_layer_set_background_color(sptRstButtonLayer, GColorWhite);

    bitmap_layer_set_background_color(ssLayer, GColorWhite);
    bitmap_layer_set_background_color(lightLayer, GColorWhite);

    text_layer_set_text_color(modeButtonLayer, colorDark);
    text_layer_set_text_color(timeChronoHhmmLayer, colorDark);
    text_layer_set_text_color(timeChronoSecLayer, colorDark);
    text_layer_set_text_color(dateInfoLayer, colorDark);
    text_layer_set_text_color(sptRstButtonLayer, colorDark);
  }

  // White foreground on dark background.
  else
  {
    text_layer_set_text_color(modeButtonLayer, GColorWhite);
    text_layer_set_text_color(timeChronoHhmmLayer, GColorWhite);
    text_layer_set_text_color(timeChronoSecLayer, GColorWhite);
    text_layer_set_text_color(dateInfoLayer, GColorWhite);
    text_layer_set_text_color(sptRstButtonLayer, GColorWhite);

    window_set_background_color(time_window, colorDark);
    text_layer_set_background_color(modeButtonLayer, colorDark);
    text_layer_set_background_color(timeChronoHhmmLayer, colorDark);
    text_layer_set_background_color(timeChronoSecLayer, colorDark);
    text_layer_set_background_color(dateInfoLayer, colorDark);
    text_layer_set_background_color(sptRstButtonLayer, colorDark);

    bitmap_layer_set_background_color(ssLayer, colorDark);
    bitmap_layer_set_background_color(lightLayer, colorDark);
  }

  // Redraw graphics based on current color option to set foreground color.
  layer_mark_dirty((Layer*)ssLayer);
  layer_mark_dirty((Layer*)lightLayer);
}


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

    tc_set_tc_layer_text();

    strftime(dateStr, 17, "%A%n%b", currentTime);
    int dayStartOff = strlen(dateStr);
    snprintf(&(dateStr[dayStartOff]), 4, " %i",  currentTime->tm_mday);
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
    // Blank when in reset state
    else
    {
      strncpy(spt_rstButtonText, BLANK_TEXT, sizeof(spt_rstButtonText));
    }

    text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  }

  // Switch to WATCH mode.
  else
  {
    layer_set_hidden(bitmap_layer_get_layer(ssLayer), true);

    strncpy(spt_rstButtonText, OPTIONS_TEXT, sizeof(spt_rstButtonText));
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

    // Redraw start/stop graphic based on current run state.
    layer_mark_dirty((Layer*)ssLayer);
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

  // CHRONO mode.
  if (selectedMode == MODE_CHRON)
  {
    // Process as split request if running.
    if (chronoRunSelect == RUN_START)
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

    // else - Ignore CHRONO mode if not running
  }

  // WATCH mode
  else
  {
    // Display options menu.
    window_stack_push(menu_window, true /* Animated */);
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

  window_long_click_subscribe(BUTTON_ID_UP, 300, (ClickHandler) tc_up_long_click_handler, NULL);

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
      //for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
      strncpy(resetButtonClearsSplits, saved_state.resetButtonClearsSplits, sizeof(resetButtonClearsSplits));
      strncpy(splitsFullReplaceOldest, saved_state.splitsFullReplaceOldest, sizeof(splitsFullReplaceOldest));
      strncpy(colorInversionChoice, saved_state.colorInversionChoice, sizeof(colorInversionChoice));
      #ifdef PBL_COLOR
      colorSelectChoice = saved_state.colorSelectChoice;
      #endif

      // Get saved extended splits before restoring all splits.
      if (persist_exists(extended_splits_key))
      {
        saved_splits_S saved_splits;
        int bytes_read = 0;
        if (sizeof(saved_splits_S) == (bytes_read = persist_read_data(extended_splits_key,
                                                                     (void *)&saved_splits,
                                                                     sizeof(saved_splits_S))))
        {
          for (int i = 0; i < BASE_SPLIT_CNT; i++)
          {
            splits[i] = saved_state.splits[i];
          }


          for (int i = 0; i < EXTENDED_SPLIT_CNT; i++)
          {
            splits[BASE_SPLIT_CNT + i] = saved_splits.splits[i];
          }

          splitIndex = saved_state.splitIndex;
        }

        // Fill in undefined fields if error reading persistent data.
        else
        {
          APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_read_data(saved_splits). bytes read = %i", bytes_read);
          for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
          {
            splits[i] = 0;
          }
        }
      }

      // Fill in undefined fields if persistent data does not exit.
      else
      {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists(saved_splits) returned false");
        for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
        {
          splits[i] = 0;
        }
      }
    }

    // Fill in undefined fields if error reading persistent data.
    else
    {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_read_data(saved_state). bytes read = %i", bytes_read);
      for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
      {
        splits[i] = 0;
      }

      strncpy(spt_rstButtonText, OPTIONS_TEXT, sizeof(spt_rstButtonText));
    }
  }

  // Fill in undefined fields if persistent data does not exist.
  else
  {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists(saved_state) returned false");
    for (int i = 0; i <= MAX_SPLIT_INDEX; i++)
    {
      splits[i] = 0;
    }

    strncpy(spt_rstButtonText, OPTIONS_TEXT, sizeof(spt_rstButtonText));
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "persistent data restore complete");

  // Fonts for time and chronometer.
  hhmm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_46));
  sec_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UNIVERS_COND_MED_24));

  // Get graphics.
  menuIcon = gbitmap_create_with_resource(RESOURCE_ID_MENU_IMAGE);
  up_image = gbitmap_create_with_resource(RESOURCE_ID_UP_ICON);
  dn_image = gbitmap_create_with_resource(RESOURCE_ID_DN_ICON);

  // SDK 3.0 support for color ionversion.
  #ifdef PBL_COLOR
    colorDark = colorSelectColor[colorSelectChoice];
  #else
    colorDark = GColorBlack;
  #endif

  // ### Menu window setup. ###
  menu_window = window_create();
  Layer * menu_window_layer = window_get_root_layer(menu_window);
  window_set_fullscreen(menu_window, true);

  menuItems[0] = (SimpleMenuItem){.title = "Display Splits",
                                  .subtitle = NULL,
                                  .callback = menuDisplaySplitsHandler,
                                  .icon = NULL};
  menuItems[1] = (SimpleMenuItem){.title = "Clear Splits",
                                  .subtitle = NULL,
                                  .callback = menuClearSplitsHandler,
                                  .icon = NULL};
  menuItems[2] = (SimpleMenuItem){.title = "Splits Option",
                                  .subtitle = NULL,
                                  .callback = menuSplitsOptionHandler,
                                  .icon = NULL};
  menuItems[3] = (SimpleMenuItem){.title = "Reset Option",
                                  .subtitle = NULL,
                                  .callback = menuResetOptionHandler,
                                  .icon = NULL};
  menuItems[4] = (SimpleMenuItem){.title = "Color Inversion",
                                  .subtitle = NULL,
                                  .callback = menuColorInversionHandler,
                                  .icon = NULL};
  #ifdef PBL_COLOR
  menuItems[5] = (SimpleMenuItem){.title = "Color Select",
                                  .subtitle = NULL,
                                  .callback = menuColorSelectHandler,
                                  .icon = NULL};
  #endif
  menuItems[NBR_MENU_ITEMS - 1] = (SimpleMenuItem){.title = APP_VERSION,
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

  // Light icon area
  lightLayer = bitmap_layer_create(GRect(82, 4, 16, 16));
  layer_set_update_proc((Layer*)lightLayer, tc_lightLayer_update_proc);
  layer_add_child(time_window_layer, bitmap_layer_get_layer(lightLayer));

  // Mode button
  modeButtonLayer = text_layer_create(GRect(98, 0, 44, 20));
  text_layer_set_text_alignment(modeButtonLayer, GTextAlignmentRight);
  text_layer_set_font(modeButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(modeButtonLayer, "/Mode");
  layer_add_child(time_window_layer, text_layer_get_layer(modeButtonLayer));

  // Time/chronograph area - Hours & Minutes
  // 2.1.1 timeChronoHhmmLayer = text_layer_create(GRect(0, 55, 102, 46));
  timeChronoHhmmLayer = text_layer_create(GRect(0, 48, 102, 46));
  text_layer_set_text_alignment(timeChronoHhmmLayer, GTextAlignmentRight);
  text_layer_set_font(timeChronoHhmmLayer, hhmm_font);

  // Time/chronograph area - Seconds
  timeChronoSecLayer = text_layer_create(GRect(104, 64, 26, 26));
  text_layer_set_text_alignment(timeChronoSecLayer, GTextAlignmentLeft);
  text_layer_set_font(timeChronoSecLayer, sec_font);

  // Time/chronograph area - common
  tc_set_tc_layer_text();
  layer_add_child(time_window_layer, text_layer_get_layer(timeChronoHhmmLayer));
  layer_add_child(time_window_layer, text_layer_get_layer(timeChronoSecLayer));

  // Start/stop area
  ssLayer = bitmap_layer_create(GRect(130, 67, 14, 30));
  layer_set_update_proc((Layer*)ssLayer, tc_ssLayer_update_proc);
  startIconP = gpath_create(&START_PATH_INFO);
  stopIconP = gpath_create(&STOP_PATH_INFO);
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
  dateInfoLayer = text_layer_create(GRect(10, 94, 120, 52));
  text_layer_set_text_alignment(dateInfoLayer, GTextAlignmentCenter);
  text_layer_set_font(dateInfoLayer, sec_font);
  text_layer_set_text(dateInfoLayer, dateStr);
  layer_add_child(time_window_layer, text_layer_get_layer(dateInfoLayer));

  // Split/Reset button
  sptRstButtonLayer = text_layer_create(GRect(0, 146, 142, 20));
  text_layer_set_text_alignment(sptRstButtonLayer, GTextAlignmentRight);
  text_layer_set_font(sptRstButtonLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(sptRstButtonLayer, spt_rstButtonText);
  layer_add_child(time_window_layer, text_layer_get_layer(sptRstButtonLayer));

  //tc_set_color();
  // Hook to call tc_set_color(). Allows it to be called just once on exit from color select.
  window_set_window_handlers(time_window, (WindowHandlers){.appear = timeAppearHandler});

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

  #ifdef PBL_COLOR
  colorSelectColor[0] = GColorFromRGB(255, 0, 0);     // red
  colorSelectColor[1] = GColorFromRGB(255, 128, 128);
  colorSelectColor[2] = GColorFromRGB(128, 0, 0);
  colorSelectColor[3] = GColorFromRGB(192, 96, 0);   // orange
  colorSelectColor[4] = GColorFromRGB(255, 192, 0);
  colorSelectColor[5] = GColorFromRGB(128, 64, 0);
  colorSelectColor[6] = GColorFromRGB(0, 0, 255);     // blue
  colorSelectColor[7] = GColorFromRGB(128, 128, 255);
  colorSelectColor[8] = GColorFromRGB(0, 0, 128);
  colorSelectColor[9] = GColorFromRGB(0, 255, 0);     // green
  colorSelectColor[10] = GColorFromRGB(128, 255, 128);
  colorSelectColor[11] = GColorFromRGB(0, 128, 0);
  colorSelectColor[12] = GColorFromRGB(128, 0, 128);   // purple
  colorSelectColor[13] = GColorFromRGB(255, 0, 255);
  colorSelectColor[14] = GColorFromRGB(96, 0, 96);
  #endif

  // Note: window_set_click_config_provider() for option window will be set based on option selected.

  // Hook to restore button labels, etc, that may need to be modified by some menu items.
  window_set_window_handlers(menu_window, (WindowHandlers){.appear = menuAppearHandler});

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
  for (int i = 0; i < BASE_SPLIT_CNT; i++)
  {
    saved_state.splits[i] = splits[i];
  }
  saved_state.splitIndex = splitIndex;
  strncpy(saved_state.resetButtonClearsSplits, resetButtonClearsSplits, sizeof(saved_state.resetButtonClearsSplits));
  strncpy(saved_state.splitsFullReplaceOldest, splitsFullReplaceOldest, sizeof(saved_state.splitsFullReplaceOldest));
  strncpy(saved_state.colorInversionChoice, colorInversionChoice, sizeof(saved_state.colorInversionChoice));
  #ifdef PBL_COLOR
  saved_state.colorSelectChoice = colorSelectChoice;
  #endif

  int bytes_written = 0;
  if (sizeof(saved_state_S) != (bytes_written = persist_write_data(persistent_data_key,
                                                                   (void *)&saved_state,
                                                                   sizeof(saved_state_S))))
  {  
    APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_write_data(saved_state). bytes written = %i", bytes_written);

    // Delete all peristent data when a problem has occurred saving any of it.
    persist_delete(extended_splits_key);
    persist_delete(persistent_data_key);
  }
  else
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "write state: selectedMode: %i, chronoRunSelect: %i, closeTm: %i",
    //                             saved_state.selectedMode,
    //                             saved_state.chronoRunSelect,
    //                             (int)saved_state.closeTm);

    // Save extended splits.
    saved_splits_S saved_splits;
    for (int i = 0; i < EXTENDED_SPLIT_CNT; i++)
    {
      saved_splits.splits[i] = splits[BASE_SPLIT_CNT + i];
    }

    bytes_written = 0; 
    if (sizeof(saved_splits_S) != (bytes_written = persist_write_data(extended_splits_key,
                                                                     (void *)&saved_splits,
                                                                     sizeof(saved_splits_S))))
    {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "error during persist_write_data(extended_splits). bytes written = %i", bytes_written);

      // Delete all peristent data when a problem has occurred saving any of it.
      persist_delete(extended_splits_key);
      persist_delete(persistent_data_key);
    }
    else
    {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "all state data saved");
    }
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
  //if (tcInverterLayer != 0)
  //{
  //  inverter_layer_destroy(tcInverterLayer);
  //  tcInverterLayer = 0;
  //}
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
  //gbitmap_destroy(light_image);
  gbitmap_destroy(menuIcon);
  //gbitmap_destroy(ss_image);
  gpath_destroy(startIconP);
  gpath_destroy(stopIconP);
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