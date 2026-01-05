# Memory Footprint Optimization

## Target Budget

| Resource | Budget | ESP32-C3 Current | CH32X035 Est. |
|----------|--------|------------------|---------------|
| **Flash** | 62 KB | 134 KB | ~45-55 KB |
| **RAM** | 20 KB | 54 KB | ~18-25 KB |

> [!WARNING]
> ESP32-C3 has ~100KB baseline overhead (IDF, HAL, drivers).
> CH32X035 should fit within budget with current optimizations.

---

## Optimization Progress (ESP32-C3)

| Configuration | Flash | RAM | Notes |
|---------------|-------|-----|-------|
| Original (dev) | 138 KB | 69 KB | Full logging, NVS, POSIX |
| + ZMS (replaces NVS) | 138 KB | 69 KB | Similar overhead |
| + Size opts + stacks | 138 KB | 56 KB | `-Os`, reduced stacks |
| **+ Logging disabled** | **134 KB** | **54 KB** | **Current best** |

### Applied Optimizations
```kconfig
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_BOOT_BANNER=n
CONFIG_CBPRINTF_NANO=y
CONFIG_MAIN_STACK_SIZE=1024
CONFIG_ISR_STACK_SIZE=1024
CONFIG_IDLE_STACK_SIZE=256
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=512
CONFIG_LOG=n
CONFIG_PRINTK=n
```

---

## CH32X035 Estimate

The CH32X035 has significantly lower baseline overhead:
- No WiFi/BT stack (ESP32-C3 has ROM IDF integration)
- Simpler HAL (no GPIO matrix routing)
- Smaller kernel footprint

### Estimated Breakdown (CH32X035)

| Component | ESP32-C3 | CH32X035 Est. |
|-----------|----------|---------------|
| Kernel core | ~25 KB | ~8-10 KB |
| HAL/drivers | ~30 KB | ~10-15 KB |
| Flash driver + ZMS | ~15 KB | ~8-10 KB |
| Application code | ~10 KB | ~10 KB |
| Strings/rodata | ~20 KB | ~5-8 KB |
| Thread stacks | ~10 KB | ~5-8 KB |
| **Total Flash** | ~134 KB | ~45-55 KB |
| **Total RAM** | ~54 KB | ~18-25 KB |

> [!TIP]
> CH32X035 with 62KB flash / 20KB RAM should fit ZBeam with room to spare.

---

## Platform Comparison

| MCU | Flash | RAM | Zephyr Support | Target Fit |
|-----|-------|-----|----------------|------------|
| ESP32-C3 | 4 MB | 400 KB | Full | ❌ Overhead too high |
| **CH32X035** | 62 KB | 20 KB | Experimental | ✅ Should fit |
| **CH32V203** | 64-256 KB | 20-64 KB | Experimental | ✅ More headroom |
| STM32G030 | 64 KB | 8 KB | Full | ⚠️ RAM tight |
| ATtiny424 | 4 KB | 512 B | None | ❌ Too small |

### CH32 Family Notes

| Variant | Flash | RAM | USB-C PD | Notes |
|---------|-------|-----|----------|-------|
| CH32X035 | 62 KB | 20 KB | ✅ Built-in | Best for USB-C flashlights |
| CH32V203F8 | 64 KB | 20 KB | ❌ | More GPIO, same memory |
| CH32V203G8 | 64 KB | 20 KB | ❌ | LQFP48 package |
| CH32V203C8 | 64 KB | 20 KB | ❌ | LQFP48, USB OTG |
| CH32V203RB | 128 KB | 64 KB | ❌ | More memory, larger package |

> [!TIP]
> **CH32X035** is ideal for USB-C powered flashlights (built-in PD controller).
> **CH32V203** offers more flexibility if USB-C PD is handled externally.

---

## Recommendations

### For ESP32-C3 (Development/High-End)
- Accept ~130KB flash / ~55KB RAM as floor
- Use for prototyping and feature-rich builds
- Keep logging enabled for debugging

### For CH32X035 (Production Target)
- Use optimized prj.conf from above
- Expect ~50KB flash / ~20KB RAM
- Validate with actual build when port is ready

---

## Measurement Commands

```bash
# Build with optimizations
west build -b <board> --pristine

# Memory reports
west build -t rom_report
west build -t ram_report

# Quick summary
size build/zephyr/zephyr.elf
```
