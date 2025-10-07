#include "../include/somakernel.h"
#include "../include/feedback_stream.h"
#include <stdio.h>

int main() {
    printf("🚀 Somakernel Test Suite\n");
    printf("========================\n");
    
    SomakernelBus* bus = somakernel_init(1024, 2048);
    printf("✅ Bus initialized with %zu segments\n", bus->ring.activeCount);

    // Submit messages to different lanes
    printf("\n📤 Submitting messages...\n");
    somakernel_submit_to(bus, 0, "Message to Lane 0", 17);
    somakernel_submit_to(bus, 1, "Message to Lane 1", 17);
    somakernel_submit_to(bus, 0, "Second message Lane 0", 21);
    somakernel_submit(bus, "Round-robin message", 19);

    printf("📥 Draining messages...\n");
    // Drain from all lanes
    for (int i = 0; i < 6; ++i) {
        printf("Drain attempt %d:\n", i + 1);
        somakernel_drain(bus);
    }

    printf("\n🧾 Feedback Summary:\n");
    printf("===================\n");
    feedback_render(&bus->feedback);
    
    printf("\n🧹 Cleanup...\n");
    somakernel_free(bus);
    printf("✅ Test completed successfully!\n");
    return 0;
}