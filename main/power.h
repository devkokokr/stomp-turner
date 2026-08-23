#pragma once

// Enables automatic (tickless-idle) light sleep: the system naps whenever
// every task is idle and no component holds a power-management lock, and
// resumes on the next scheduled wakeup (timer tick, GPIO/UART interrupt,
// etc.) -- no explicit wakeup-source configuration needed. The NimBLE
// controller acquires/releases its own lock around active radio windows, so
// this is safe to run continuously with a live BLE connection.
//
// This deliberately does NOT implement firmware.md's originally-sketched
// "5 minutes idle -> forced esp_light_sleep_start() with GP4/GP5 as an
// explicit wakeup source": that call bypasses the power-management lock the
// BLE controller relies on to avoid being suspended mid-radio-operation, and
// could destabilize or drop the connection in ways that can't be verified
// without hardware. If bench current-draw measurements show automatic light
// sleep isn't saving enough for the target battery life, revisit with a
// graceful BLE-aware deeper-sleep path instead of a raw forced call.
//
// Call once from app_main(), after nvs_flash/BLE bring-up order doesn't
// matter (locks are acquired/released at runtime, not at init time).
void power_init(void);
