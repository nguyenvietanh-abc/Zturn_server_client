#include "Client.h"

int main() {
    Client client("192.168.1.3", 8080, INFO);

    if (!client.connectToServer()) {
        return -1;
    }

    client.start();
    client.stop();

    return 0;
}
