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

### Actual Build (CH32V303VCT6 - ZBeam Core, No NVS)

| Component | Code (Flash) | RAM |
|-----------|--------------|-----|
| Base Logic (Core only) | 27.7 KB | 14.8 KB |
| With USB Stack (Config) | 29.0 KB | 14.9 KB |
| **Delta (USB Overhead)** | **+1.3 KB** | **+0.1 KB** |

> [!NOTE]
> - **USB Overhead**: Adding `CONFIG_USB_DEVICE_STACK` added very little overhead (1.3KB).
> - **Total Footprint**: ~29KB Flash / ~15KB RAM is well within the 62KB/20KB limit of the CH32X035.
> - **Conclusion**: Even with USB enabled, we have >30KB (50%) Flash remaining for NVS, PD logic, and more features.

---

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
