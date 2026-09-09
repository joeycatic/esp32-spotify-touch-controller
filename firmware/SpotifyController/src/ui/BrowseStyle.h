#pragma once

#include <lvgl.h>

namespace spotctl {

// Shared styles keep the per-playlist allocation small on the device.
inline void styleBrowseRow(lv_obj_t *row, bool highlighted = false) {
  static lv_style_t base;
  static lv_style_t pressed;
  static lv_style_t selected;
  static bool initialized = false;
  if (!initialized) {
    lv_style_init(&base);
    lv_style_set_bg_color(&base, lv_color_hex(0x15191E));
    lv_style_set_bg_opa(&base, LV_OPA_COVER);
    lv_style_set_text_color(&base, lv_color_hex(0xF7F7F7));
    lv_style_set_border_width(&base, 1);
    lv_style_set_border_color(&base, lv_color_hex(0x242B32));
    lv_style_set_radius(&base, 14);
    lv_style_set_shadow_width(&base, 0);
    lv_style_init(&pressed);
    lv_style_set_bg_color(&pressed, lv_color_hex(0x243A2E));
    lv_style_set_border_color(&pressed, lv_color_hex(0x1ED760));
    lv_style_init(&selected);
    lv_style_set_bg_color(&selected, lv_color_hex(0x182A22));
    lv_style_set_text_color(&selected, lv_color_hex(0x1ED760));
    lv_style_set_border_color(&selected, lv_color_hex(0x285C3B));
    initialized = true;
  }
  lv_obj_add_style(row, &base, 0);
  if (highlighted) {
    lv_obj_add_style(row, &selected, 0);
  }
  lv_obj_add_style(row, &pressed, LV_STATE_PRESSED);
}

inline void styleBrowseList(lv_obj_t *list, bool wide, bool library) {
  lv_obj_set_style_bg_color(list, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_radius(list, 0, 0);
  lv_obj_set_style_pad_all(list, wide ? 4 : 2, 0);
  lv_obj_set_style_pad_right(list, wide ? 12 : 2, 0);
  lv_obj_set_style_pad_row(list, wide ? 12 : 6, 0);
  lv_obj_set_style_pad_column(list, 16, 0);
  lv_obj_set_flex_flow(list, wide && library ? LV_FLEX_FLOW_ROW_WRAP
                                            : LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x52605A), LV_PART_SCROLLBAR);
  lv_obj_set_style_width(list, 3, LV_PART_SCROLLBAR);
}

inline lv_obj_t *makeWideBrowseRow(lv_obj_t *list, const char *title,
                                   const char *detail, const char *trailing,
                                   bool library, bool highlighted = false) {
  static lv_style_t row_styles[2];
  static lv_style_t name_styles[2];
  static lv_style_t detail_styles[2];
  static lv_style_t icon_style;
  static lv_style_t action_style;
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < 2; ++i) {
      const bool card = i == 1;
      lv_style_init(&row_styles[i]);
      lv_style_set_width(&row_styles[i], card ? 456 : lv_pct(100));
      lv_style_set_height(&row_styles[i], card ? 104 : 88);
      lv_style_set_pad_all(&row_styles[i], 0);
      lv_style_set_layout(&row_styles[i], 0);
      lv_style_init(&name_styles[i]);
      lv_style_set_x(&name_styles[i], 96);
      lv_style_set_y(&name_styles[i], card ? 22 : 15);
      lv_style_set_width(&name_styles[i], card ? 304 : 704);
      lv_style_set_height(&name_styles[i], 25);
      lv_style_set_text_font(&name_styles[i], &lv_font_montserrat_20);
      lv_style_init(&detail_styles[i]);
      lv_style_set_x(&detail_styles[i], 96);
      lv_style_set_y(&detail_styles[i], card ? 56 : 47);
      lv_style_set_width(&detail_styles[i], card ? 304 : 704);
      lv_style_set_height(&detail_styles[i], 18);
      lv_style_set_text_font(&detail_styles[i], &lv_font_montserrat_14);
      lv_style_set_text_color(&detail_styles[i], lv_color_hex(0xA6ABB2));
    }
    lv_style_init(&icon_style);
    lv_style_set_align(&icon_style, LV_ALIGN_LEFT_MID);
    lv_style_set_x(&icon_style, 16);
    lv_style_set_text_color(&icon_style, lv_color_hex(0x7F9188));
    lv_style_set_text_font(&icon_style, &lv_font_montserrat_24);
    lv_style_init(&action_style);
    lv_style_set_align(&action_style, LV_ALIGN_RIGHT_MID);
    lv_style_set_x(&action_style, -20);
    lv_style_set_text_font(&action_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&action_style, lv_color_hex(0x8AA493));
    initialized = true;
  }
  const int variant = library ? 1 : 0;
  lv_obj_t *row = lv_btn_create(list);
  lv_obj_add_style(row, &row_styles[variant], 0);
  styleBrowseRow(row, highlighted);

  // The thumbnail cache expects the first child to be an image. Text starts
  // beyond a reserved 64px slot, so loading artwork never moves the labels.
  lv_obj_t *icon = lv_img_create(row);
  lv_obj_add_style(icon, &icon_style, 0);
  lv_img_set_src(icon, library ? LV_SYMBOL_AUDIO : LV_SYMBOL_PLAY);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *name = lv_label_create(row);
  lv_obj_add_style(name, &name_styles[variant], 0);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_label_set_text(name, title);

  lv_obj_t *subtitle = lv_label_create(row);
  lv_obj_add_style(subtitle, &detail_styles[variant], 0);
  lv_label_set_long_mode(subtitle, LV_LABEL_LONG_DOT);
  lv_label_set_text(subtitle, detail);

  lv_obj_t *action = lv_label_create(row);
  lv_obj_add_style(action, &action_style, 0);
  lv_label_set_text(action, trailing);
  return row;
}

} // namespace spotctl
