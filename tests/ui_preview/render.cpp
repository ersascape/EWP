// Draw the production screens into Adafruit_GFX's 200x200 host canvas.
#include <Adafruit_GFX.h>
#include "apps/app_notifications.h"
#include "apps/app_call.h"
#include "apps/app_now_playing.h"
#include "ersa/services/bluetooth_manager.h"
#include "mocks/mock_bluetooth.h"
#include <stdio.h>
#include <assert.h>
#include <string>

namespace DebugLog { void log(const char*, ...) {} }

static void save(const GFXcanvas1& canvas, const std::string& path) {
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) return;
    fprintf(file, "P5\n200 200\n255\n");
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 200; ++x)
            fputc(canvas.getPixel(x, y) ? 255 : 0, file);
    fclose(file);
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string directory = argv[1];
    ersa::events::EventBus bus;
    ersa::test::MockBluetooth provider;
    ersa::services::BluetoothManager manager(provider, bus);
    ersa::services::BluetoothManager::setInstance(&manager);
    manager.init();
    provider.simulateConnection(true);
    GFXcanvas1 canvas(200, 200);

    provider.simulateNotification("Messages", "Meeting starts in 5 minutes", "Messages", 42);
    AppNotifications::render(canvas, false);
    save(canvas, directory + "/notification.pgm");
    assert(AppNotifications::onButton(Buttons::Event::Action));
    assert(manager.getNotificationCount() == 0);
    provider.simulateNotification("Messages", "late update", "Messages", 42);
    assert(manager.getNotificationCount() == 0);

    provider.simulateIncomingCall("Alexandria Montgomery", "");
    AppCall::render(canvas);
    save(canvas, directory + "/call.pgm");

    provider.simulateMedia(true, "Everything In Its Right Place", "Radiohead");
    AppNowPlaying::render(canvas);
    save(canvas, directory + "/now_playing.pgm");
}
