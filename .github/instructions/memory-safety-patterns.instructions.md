---
applyTo: "firmware/main/**/*.{c,h}"
---

# @memory-safety-patterns

Cross-language patterns for memory-safe programming including RAII-style resource management, ownership, and resource cleanup. Applies to all C source and header files in the ESP32-C6 firmware.

## Use this skill when

- Writing code that allocates memory, opens files, or acquires mutexes/semaphores
- Managing LVGL widget lifecycles or Zigbee stack buffers
- Preventing use-after-free and memory leaks
- Debugging heap corruption or double-free crashes

## Core Patterns for Embedded C

### RAII-Style Resource Management

C doesn't have destructors, but you can enforce cleanup discipline:

```c
// Acquire all resources at the top; one cleanup path at the bottom
esp_err_t do_work(void) {
    uint8_t *buf = NULL;
    SemaphoreHandle_t lock = NULL;
    esp_err_t ret = ESP_OK;

    buf = malloc(BUF_SIZE);
    if (!buf) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    lock = xSemaphoreCreateMutex();
    if (!lock) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    // ... work ...

cleanup:
    vSemaphoreDelete(lock);  // safe: vSemaphoreDelete(NULL) is a no-op
    free(buf);               // safe: free(NULL) is a no-op
    return ret;
}
```

### Ownership Rules

Annotate ownership in function signatures:

```c
// Caller owns returned buffer — must free()
uint8_t *zigbee_build_frame(const payload_t *p);

// Callee owns — do NOT free the returned pointer
const config_t *config_get_defaults(void);

// Transfer ownership — callee frees
void queue_send_take(QueueHandle_t q, void *data);
```

### Null Pointer Defense

Always initialize pointers to `NULL` and check before use:

```c
display_handle_t *disp = NULL;
disp = display_init();
if (!disp) { return ESP_FAIL; }
// use disp safely
```

Set to `NULL` after free to prevent use-after-free:

```c
free(buf);
buf = NULL;
```

### Buffer Bounds

Prefer size-bounded string operations:

```c
// ❌
sprintf(cmd, "AT+SSID=%s", ssid);

// ✅
snprintf(cmd, sizeof(cmd), "AT+SSID=%s", ssid);
```

### FreeRTOS Resource Safety

- Always pair `xSemaphoreTake` with `xSemaphoreGive` on all code paths
- Never call blocking FreeRTOS APIs from ISR context — use `FromISR` variants
- Delete tasks with `vTaskDelete(NULL)` at the end of the task function; never return from a task

### LVGL Memory

- LVGL objects are owned by their parent; deleting a parent frees all children
- Never store raw `lv_obj_t *` in long-lived structures without registering a delete callback
- Use `lv_obj_add_event_cb(obj, cb, LV_EVENT_DELETE, NULL)` to clean up associated state

## Debugging Memory Issues

- Enable `CONFIG_HEAP_POISONING_COMPREHENSIVE` in sdkconfig for heap corruption detection
- Use `heap_caps_check_integrity_all(true)` at strategic points
- `esp_heap_trace_start()` / `esp_heap_trace_stop()` for leak hunting
