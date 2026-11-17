
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <modbus/modbus.h>

int main() {
    modbus_t *ctx = modbus_new_tcp("127.0.0.1", 5020);
    assert(ctx != NULL);
    modbus_set_slave(ctx, 1);

    if (modbus_connect(ctx) == -1) {
        printf("FAIL: Could not connect to server on port 5020\n");
        return 1;
    }
    printf("PASS: Connected to Modbus Server (Port 5020, Unit ID 1)\n");

    // 1. Send START command (1) to Holding Register 0
    assert(modbus_write_register(ctx, 0, 1) == 1);

    // 2. Check Holding Register 0 resets to 0 automatically
    uint16_t hold_reg[3];
    assert(modbus_read_registers(ctx, 0, 3, hold_reg) == 3);
    assert(hold_reg[0] == 0);
    printf("PASS: START sent and Holding Register 0 reset to 0\n");

    // Wait 2 seconds for pump RPM to rise
    sleep(2);

    // 3. Read Discrete Inputs (0 = Running, 1 = Fault)
    uint8_t disc_in[2];
    assert(modbus_read_input_bits(ctx, 0, 2, disc_in) == 2);
    assert(disc_in[0] == 1);
    assert(disc_in[1] == 0);
    printf("PASS: Discrete Inputs show Pump is RUNNING\n");

    // 4. Read Input Registers (0 to 5)
    uint16_t in_reg[6];
    assert(modbus_read_input_registers(ctx, 0, 6, in_reg) == 6);
    assert(in_reg[3] > 0);
    printf("PASS: Input Registers read (RPM = %d, Tank = %.1f%%)\n",
           in_reg[3], in_reg[0] / 10.0f);

    modbus_close(ctx);
    modbus_free(ctx);
    printf("\nALL MODBUS INTEGRATION TESTS PASSED!\n");
    return 0;
}