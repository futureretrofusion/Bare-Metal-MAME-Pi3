#ifndef FRF_GUI_H
#define FRF_GUI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FRF_GUI_ROWS 14
typedef struct {
    const char *title;
    const char *short_name;
    unsigned status; /* 0 untested, 1 returned cleanly, 2 failure */
    unsigned rom_status; /* 1 exact, 2 aliases, 3 missing, 4 undumped */
} frf_gui_item;
typedef struct {
    const char *selected_title;
    const char *selected_short;
    const char *selected_year;
    const char *selected_maker;
    const char *last_performance;
    const char *last_message;
    unsigned count, selected, first, rows;
    unsigned total_count, filter_mode;
    unsigned native_width, native_height, native_millihz;
    unsigned frame_skip_mode, double_buffer, usb_keyboard, usb_gamepad;
    unsigned usb_pad_connected, usb_pad_vidpid, usb_pad_reports;
    unsigned usb_pad_mask, usb_pad_decoder, usb_pad_report_id;
    unsigned usb_host_ok,usb_device_count,usb_enum_errors;
    unsigned usb_any_vidpid,usb_any_addr;
    unsigned selected_status;
    unsigned selected_rom_status;
    frf_gui_item items[FRF_GUI_ROWS];
} frf_gui_model;
void frf_gui_attach(void *pixels, unsigned width, unsigned height,
                    unsigned pitch, unsigned depth);
int frf_gui_available(void);
/* Menu-only double-buffered scanout. Game renderer retains ownership
   of its existing page flip path. */
void frf_gui_configure_double(unsigned virt_height, unsigned fb_bytes,
                              int (*flip)(unsigned page));
void frf_gui_before_game(void);
void frf_gui_after_game(void);
int frf_gui_is_double(void);
unsigned frf_gui_failed_flips(void);
void frf_gui_render(const frf_gui_model *m);
#ifdef __cplusplus
}
#endif
#endif
