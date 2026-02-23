# Skill: SPD2010 Touch Panel — Custom I2C Driver

## Problem
The managed component `espressif__esp_lcd_touch_spd2010` v2.0.0 doesn't work with ESP-IDF v5.5's 
new `i2c_master` driver. The `panel_io_i2c_v2` implementation calls `i2c_master_transmit_receive()` 
with a zero-length write buffer, which the new driver rejects.

## Root Cause
The SPD2010 touch driver config sets `disable_control_phase=1` and `lcd_cmd_bits=0`, meaning no 
command bytes should precede the read. But `panel_io_i2c_v2` still calls `i2c_master_transmit_receive()` 
even with these settings, and the new i2c_master driver validates `write_size > 0`.

## Solution
Bypass `esp_lcd_panel_io` entirely. Use `i2c_master_transmit()` + `i2c_master_receive()` directly.

### SPD2010 I2C Protocol
- **Address**: 0x53 on I2C1 (SDA=39, SCL=38, 400kHz)
- **Write**: Send 2+ bytes (register address + data)
- **Read**: Write 2-byte register address → wait 200μs → read N bytes (separate transactions)
- **Commands**: 4 bytes (cmd, b1, b2, b3)

### Key Functions
```c
static i2c_master_dev_handle_t s_touch_i2c_dev;

static esp_err_t spd2010_i2c_write(const uint8_t *data, size_t len);
static esp_err_t spd2010_i2c_read(uint8_t *data, size_t len);
static esp_err_t spd2010_read_fw_version(void);
static esp_err_t spd2010_write_cmd(uint8_t cmd, uint8_t b1, uint8_t b2, uint8_t b3);
static esp_err_t spd2010_clear_int(void);   // cmd=0x02
static esp_err_t spd2010_cpu_start(void);   // cmd=0x04
static esp_err_t spd2010_point_mode(void);  // cmd=0x50
static esp_err_t spd2010_touch_start(void); // cmd=0x46
static esp_err_t spd2010_read_data_impl(esp_lcd_touch_handle_t tp);
static bool spd2010_get_xy_impl(esp_lcd_touch_handle_t tp, ...);
static esp_err_t spd2010_del_impl(esp_lcd_touch_handle_t tp);
```

### Boot Sequence
The SPD2010 goes through: BIOS → CPU transition. During boot:
1. First reads may NACK (chip not ready) — handle gracefully, return ESP_OK
2. `tic_in_bios` flag: send `clear_int()` + `cpu_start()`
3. `tic_in_cpu` flag: send `point_mode()` + `touch_start()` + `clear_int()`
4. After transition, reads succeed consistently

### Integration with LVGL
Create `esp_lcd_touch_handle_t` manually with custom function pointers:
```c
esp_lcd_touch_handle_t tp = heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
tp->read_data = spd2010_read_data_impl;
tp->get_xy = spd2010_get_xy_impl;
tp->del = spd2010_del_impl;
tp->config.x_max = 412;
tp->config.y_max = 412;
// Then use: lvgl_port_add_touch(tp)
```

### Touch Events for UI
- Make LVGL objects clickable: `lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE)`
- Add callbacks: `lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, NULL)`
- Screen-wide touch: `lv_obj_add_event_cb(screen, callback, LV_EVENT_PRESSED, NULL)`
- Use event group bits to decouple UI from main app logic

## Files Modified
- `components/board/board.c` — Custom SPD2010 driver (~150 lines)
- Changed include: `esp_lcd_touch.h` + `rom/ets_sys.h` (not `esp_lcd_touch_spd2010.h`)
