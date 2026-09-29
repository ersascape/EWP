#pragma once

namespace Dvfs {

enum class Profile : unsigned char {
    Interactive, // ESP_PM_APB_FREQ_MAX: CPU 80 MHz, APB 80 MHz
    Compute      // ESP_PM_CPU_FREQ_MAX: CPU 160 MHz
};

bool begin();
void tick(); // Samples CPU frequency while application code is running.
void reportPowerModes(); // One-time ESP-IDF PM lock and frequency residency report.

class Scope {
public:
    Scope(Profile profile, const char* reason);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Profile profile_;
    const char* reason_;
    bool acquired_;
};

} // namespace Dvfs
