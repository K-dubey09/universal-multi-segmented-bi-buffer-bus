#include "../include/somakernel.h"
#include "../include/feedback_stream.h"

int main() {
    SomakernelBus* bus = somakernel_init(1024, 2048);

    somakernel_submit(bus, "Capsule A", 9);
    somakernel_submit(bus, "Capsule B", 9);
    somakernel_submit(bus, "Capsule C", 9);

    for (int i = 0; i < 3; ++i) {
        somakernel_drain(bus);
    }

    feedback_render(&bus->feedback);
    somakernel_free(bus);
    return 0;
}