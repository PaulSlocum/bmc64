/*
 * vice_api.c - VICE specific impl of emux_api.h
 *
 * Written by
 *  Randy Rossi <randy.rossi@gmail.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "emux_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// VICE includes
#include "autostart.h"
#include "attach.h"
#include "cartridge.h"
#include "interrupt.h"
#include "machine.h"
#include "videoarch.h"
#include "menu.h"
#include "menu_timing.h"
#include "ui.h"
#include "keyboard.h"
#include "demo.h"
#include "datasette.h"
#include "resources.h"
#include "drive.h"

// RASPI includes
#include "circle.h"

static void pause_trap(uint16_t addr, void *data) {
  menu_about_to_activate();
  circle_show_fbl(FB_LAYER_UI);
  while (ui_enabled) {
    circle_check_gpio();
    ui_check_key();

    ui_handle_toggle_or_quick_func();

    ui_render_single_frame();
    hdmi_timing_hook();
    ensure_video();
  }
  menu_about_to_deactivate();
  circle_hide_fbl(FB_LAYER_UI);
}

void emu_machine_init(void) {
  switch (machine_class) {
    case VICE_MACHINE_C64:
       emux_machine_class = BMC64_MACHINE_CLASS_C64;
       break;
    case VICE_MACHINE_C128:
       emux_machine_class = BMC64_MACHINE_CLASS_C128;
       break;
    case VICE_MACHINE_VIC20:
       emux_machine_class = BMC64_MACHINE_CLASS_VIC20;
       break;
    case VICE_MACHINE_PLUS4:
       emux_machine_class = BMC64_MACHINE_CLASS_PLUS4;
       break;
    default:
       assert(0);
       break;
  }
}

void emux_trap_main_loop_ui(void) {
  interrupt_maincpu_trigger_trap(pause_trap, 0);
}

void emux_trap_main_loop(void (*trap_func)(uint16_t, void *data), void* data) {
  interrupt_maincpu_trigger_trap(trap_func, data);
}

void emux_kbd_set_latch_keyarr(int row, int col, int value) {
  demo_reset_timeout();
  keyboard_set_keyarr(row, col, value);
}

int emux_attach_disk_image(int unit, char* filename) {
  return file_system_attach_disk(unit, filename);
}

void emux_detach_disk(int unit) {
  file_system_detach_disk(unit);
}

int emux_attach_cart(int bank, char* filename) {
  // Ignore bank for vice
  return cartridge_attach_image(CARTRIDGE_CRT, filename);
}

void emux_detach_cart(int bank) {
  // Ignore bank for vice
  cartridge_detach_image(CARTRIDGE_NONE);
}

void emux_set_cart_default(void) {
   cartridge_set_default();
}

void emux_reset(int soft) {
  machine_trigger_reset(soft ? 
      MACHINE_RESET_MODE_SOFT : MACHINE_RESET_MODE_HARD);
}

int emux_save_state(char *filename) {
  return machine_write_snapshot(filename, 1, 1, 0);
}

int emux_load_state(char *filename) {
  return machine_read_snapshot(filename, 0);
}

int emux_tape_control(int cmd) {
  switch (cmd) {
    case EMUX_TAPE_PLAY:
      datasette_control(DATASETTE_CONTROL_START);
      break;
    case EMUX_TAPE_STOP:
      datasette_control(DATASETTE_CONTROL_STOP);
      break;
    case EMUX_TAPE_REWIND:
      datasette_control(DATASETTE_CONTROL_REWIND);
      break;
    case EMUX_TAPE_FASTFORWARD:
      datasette_control(DATASETTE_CONTROL_FORWARD);
      break;
    case EMUX_TAPE_RECORD:
      datasette_control(DATASETTE_CONTROL_RECORD);
      break;
    case EMUX_TAPE_RESET:
      datasette_control(DATASETTE_CONTROL_RESET);
      break;
    case EMUX_TAPE_ZERO:
      datasette_control(DATASETTE_CONTROL_RESET_COUNTER);
      break;
    default:
      assert(0);
      break;
  }
}

int emux_autostart_file(char* filename) {
   return autostart_autodetect(filename, NULL, 0, AUTOSTART_MODE_RUN);
}

void emux_drive_change_model(int unit) {
  struct menu_item *model_root = ui_push_menu(12, 8);
  struct menu_item *item;

  int current_drive_type;
  resources_get_int_sprintf("Drive%iType", &current_drive_type, unit);

  item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "None");
  item->value = DRIVE_TYPE_NONE;
  if (current_drive_type == DRIVE_TYPE_NONE) {
    strcat(item->displayed_value, " (*)");
  }

  if (drive_check_type(DRIVE_TYPE_1541, unit - 8) > 0) {
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "1541");
    item->value = DRIVE_TYPE_1541;
    if (current_drive_type == DRIVE_TYPE_1541) {
      strcat(item->displayed_value, " (*)");
    }
  }
  if (drive_check_type(DRIVE_TYPE_1541II, unit - 8) > 0) {
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "1541II");
    item->value = DRIVE_TYPE_1541II;
    if (current_drive_type == DRIVE_TYPE_1541II) {
      strcat(item->displayed_value, " (*)");
    }
  }
  if (drive_check_type(DRIVE_TYPE_1551, unit - 8) > 0) {
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "1551");
    item->value = DRIVE_TYPE_1551;
    if (current_drive_type == DRIVE_TYPE_1551) {
      strcat(item->displayed_value, " (*)");
    }
  }
  if (drive_check_type(DRIVE_TYPE_1571, unit - 8) > 0) {
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "1571");
    item->value = DRIVE_TYPE_1571;
    if (current_drive_type == DRIVE_TYPE_1571) {
      strcat(item->displayed_value, " (*)");
    }
  }
  if (drive_check_type(DRIVE_TYPE_1581, unit - 8) > 0) {
    item = ui_menu_add_button(MENU_DRIVE_MODEL_SELECT, model_root, "1581");
    item->value = DRIVE_TYPE_1581;
    if (current_drive_type == DRIVE_TYPE_1581) {
      strcat(item->displayed_value, " (*)");
    }
  }
}

void emux_add_parallel_cable_option(struct menu_item* parent,
                                    int id, int drive) {
  if (emux_machine_class != BMC64_MACHINE_CLASS_C64 &&
      emux_machine_class != BMC64_MACHINE_CLASS_C128) {
    return;
  }

  int tmp;
  resources_get_int_sprintf("Drive%iParallelCable", &tmp, drive);

  int index = 0;
  switch (tmp) {
    case DRIVE_PC_NONE:
       index = 0; break;
    case DRIVE_PC_STANDARD:
       index = 1; break;
    case DRIVE_PC_DD3:
       index = 2; break;
    case DRIVE_PC_FORMEL64:
       index = 3; break;
    default:
       return;
  }

  struct menu_item* child =
      ui_menu_add_multiple_choice(id, parent, "Parallel Cable");
  child->num_choices = 4;
  child->value = index;
  strcpy(child->choices[0], "None");
  strcpy(child->choices[1], "Standard");
  strcpy(child->choices[2], "Dolphin DOS");
  strcpy(child->choices[3], "Formel 64");
  child->choice_ints[0] = DRIVE_PC_NONE;
  child->choice_ints[1] = DRIVE_PC_STANDARD;
  child->choice_ints[2] = DRIVE_PC_DD3;
  child->choice_ints[3] = DRIVE_PC_FORMEL64;
}